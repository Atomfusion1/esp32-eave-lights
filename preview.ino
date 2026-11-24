// preview - shows unapplied changes in the live view without changing the real lights.
//
// While a preview is active, every frame is drawn twice:
//   1. normally -> out[] -> the real lights
//   2. with the preview settings swapped into the target channel(s) -> previewOut[]
// GET /api/frame?preview=1 returns previewOut[] (the page shows it marked PREVIEW).
// The preview uses the real effect code, so it looks exactly like Apply would.
//
// Each frame both renders start from the same effect positions, so the other channels
// look identical in both; the preview channel keeps its own positions in pv*[] so its
// animation runs on smoothly. The preview always shows the target turned on.
// It stops on POST /api/preview stop=1 (page: Apply / Discard) or after PREVIEW_TIMEOUT_MS.

const uint32_t PREVIEW_TIMEOUT_MS = 3UL * 60 * 1000;

bool       previewActive = false;
int        previewTarget = -1;        // -1 = both strands, else a channel
LightState previewState;
uint32_t   previewLastMs = 0;
CRGB       previewOut[NUM_OUTPUTS][MAX_LEDS];

// Effect positions: before this frame's normal render, after it, and the preview's own.
uint16_t pvSavedFade[NUM_CHANNELS], pvAfterFade[NUM_CHANNELS], pvFade[NUM_CHANNELS];
uint32_t pvSavedMotion[NUM_CHANNELS], pvAfterMotion[NUM_CHANNELS], pvMotion[NUM_CHANNELS];
uint32_t pvSavedEffect[NUM_CHANNELS], pvAfterEffect[NUM_CHANNELS], pvEffect[NUM_CHANNELS];

bool previewCovers(int ch) {
  return previewTarget < 0 ? ch < NUM_OUTPUTS : ch == previewTarget;
}

// Called by renderFrame() before the normal render.
void previewBeforeFrame() {
  if (previewActive && millis() - previewLastMs > PREVIEW_TIMEOUT_MS) previewActive = false;
  if (!previewActive) return;
  memcpy(pvSavedFade, fadePhase, sizeof(fadePhase));
  memcpy(pvSavedMotion, motionPos, sizeof(motionPos));
  memcpy(pvSavedEffect, effectMs, sizeof(effectMs));
}

// Called by renderFrame() after the normal frame has been sent to the lights.
void previewAfterFrame(uint32_t dtMs) {
  if (!previewActive) return;

  // remember where the real effects are now, then rewind to the start of this frame
  memcpy(pvAfterFade, fadePhase, sizeof(fadePhase));
  memcpy(pvAfterMotion, motionPos, sizeof(motionPos));
  memcpy(pvAfterEffect, effectMs, sizeof(effectMs));
  memcpy(fadePhase, pvSavedFade, sizeof(fadePhase));
  memcpy(motionPos, pvSavedMotion, sizeof(motionPos));
  memcpy(effectMs, pvSavedEffect, sizeof(effectMs));

  // swap in the preview settings and the preview's own positions
  LightState realStates[NUM_OUTPUTS > 1 ? NUM_OUTPUTS : 1];
  LightState realOne = states[0];
  if (previewTarget < 0) {
    for (int o = 0; o < NUM_OUTPUTS; o++) realStates[o] = states[o];
  } else {
    realOne = states[previewTarget];
  }
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    if (!previewCovers(ch)) continue;
    states[ch] = previewState;
    fadePhase[ch] = pvFade[ch];
    motionPos[ch] = pvMotion[ch];
    effectMs[ch]  = pvEffect[ch];
  }

  drawLayers(dtMs);
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    for (int i = 0; i < ledCount[o]; i++) previewOut[o][i] = capPixel(frame[o][i]);
  }

  // keep the preview's positions, put everything real back
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    if (!previewCovers(ch)) continue;
    pvFade[ch] = fadePhase[ch];
    pvMotion[ch] = motionPos[ch];
    pvEffect[ch] = effectMs[ch];
  }
  if (previewTarget < 0) {
    for (int o = 0; o < NUM_OUTPUTS; o++) states[o] = realStates[o];
  } else {
    states[previewTarget] = realOne;
  }
  memcpy(fadePhase, pvAfterFade, sizeof(fadePhase));
  memcpy(motionPos, pvAfterMotion, sizeof(motionPos));
  memcpy(effectMs, pvAfterEffect, sizeof(effectMs));
}

// POST /api/preview  output=<as /api/state> + any look fields  -> start/update the preview
//                    stop=1                                     -> end it
void handlePreview() {
  if (server.hasArg("stop")) {
    previewActive = false;
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  int target;
  if (!parseTarget(target)) return sendError("output must be all, a, b, house or a zone");
  LightState ps = states[target < 0 ? 0 : target];
  bool changed = false;
  const char* err = applyLookArgs(ps, changed);
  if (err) return sendError(err);
  ps.on = true;   // a preview always shows the target on

  if (!previewActive || target != previewTarget) {
    // new preview: its animation starts from the beginning
    memset(pvFade, 0, sizeof(pvFade));
    memset(pvMotion, 0, sizeof(pvMotion));
    memset(pvEffect, 0, sizeof(pvEffect));
  }
  previewState = ps;
  previewTarget = target;
  previewActive = true;
  previewLastMs = millis();
  server.send(200, "application/json", "{\"ok\":true}");
}
