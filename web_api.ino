// web_api — control page and JSON API on port 80.
//
//   GET  /            control page (on/off, color, brightness, mode)
//   GET  /api/state   current state as JSON
//   POST /api/state   change any of: on=0|1, color=RRGGBB (or #RRGGBB),
//                     brightness=0-255, mode=solid|fade|test,
//                     fadeSeconds=2-60, fadeMin=0-255
//                     (form-encoded body or URL query); returns the new state
//
// No login: anyone on the same network can use it. Fine on a home network.

WebServer server(80);

const char* MODE_NAMES[MODE_COUNT] = { "solid", "fade", "test" };

String stateJson() {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"on\":%s,\"color\":\"#%02X%02X%02X\",\"brightness\":%u,"
           "\"mode\":\"%s\",\"fadeSeconds\":%u,\"fadeMin\":%u,\"version\":\"%s\"}",
           state.on ? "true" : "false",
           state.color.r, state.color.g, state.color.b,
           state.brightness, MODE_NAMES[state.mode],
           state.fadeSeconds, state.fadeMin, FW_VERSION);
  return String(buf);
}

void handleGetState() {
  server.send(200, "application/json", stateJson());
}

void handleSetState() {
  bool changed = false;

  if (server.hasArg("on")) {
    String v = server.arg("on");
    state.on = (v == "1" || v == "true" || v == "on");
    changed = true;
  }
  if (server.hasArg("color")) {
    String v = server.arg("color");
    if (v.startsWith("#")) v = v.substring(1);
    if (v.length() != 6) {
      server.send(400, "application/json", "{\"error\":\"color must be RRGGBB\"}");
      return;
    }
    state.color = CRGB((uint32_t)strtoul(v.c_str(), nullptr, 16));
    changed = true;
  }
  if (server.hasArg("brightness")) {
    state.brightness = (uint8_t)constrain(server.arg("brightness").toInt(), 0, 255);
    changed = true;
  }
  if (server.hasArg("mode")) {
    String v = server.arg("mode");
    int found = -1;
    for (int i = 0; i < MODE_COUNT; i++) {
      if (v == MODE_NAMES[i]) found = i;
    }
    if (found < 0) {
      server.send(400, "application/json", "{\"error\":\"mode must be solid, fade or test\"}");
      return;
    }
    state.mode = (Mode)found;
    changed = true;
  }
  if (server.hasArg("fadeSeconds")) {
    state.fadeSeconds = (uint8_t)constrain(server.arg("fadeSeconds").toInt(),
                                           FADE_SECONDS_MIN, FADE_SECONDS_MAX);
    changed = true;
  }
  if (server.hasArg("fadeMin")) {
    state.fadeMin = (uint8_t)constrain(server.arg("fadeMin").toInt(), 0, 255);
    changed = true;
  }

  if (changed) markStateChanged();
  server.send(200, "application/json", stateJson());
}

const char INDEX_HTML[] PROGMEM = R"rawhtml(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Eave Lights</title>
<style>
:root{--bg:#f4f4f2;--card:#fff;--text:#1d1d1b;--muted:#6b6b66;--accent:#d9822b;--line:#deded9}
@media (prefers-color-scheme:dark){:root{--bg:#141413;--card:#1f1f1d;--text:#ececea;--muted:#9a9a94;--accent:#e8954a;--line:#33332f}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,sans-serif}
main{max-width:420px;margin:0 auto;padding:20px 16px}
h1{font-size:22px;margin:0 0 16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:14px}
label{display:block;color:var(--muted);font-size:13px;margin-bottom:8px}
button.power{width:100%;padding:16px;font-size:18px;font-weight:600;border-radius:12px;border:none;cursor:pointer;background:var(--line);color:var(--text)}
button.power.on{background:var(--accent);color:#fff}
input[type=color]{width:100%;height:56px;border:none;border-radius:10px;background:none;padding:0}
input[type=range]{width:100%;accent-color:var(--accent)}
.modes{display:flex;gap:8px}
.modes button{flex:1;padding:12px;border-radius:10px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer}
.modes button.sel{border-color:var(--accent);background:var(--accent);color:#fff}
.status{color:var(--muted);font-size:13px;text-align:center}
.hint{color:var(--muted);font-size:12px;margin:8px 0 0}
</style></head><body><main>
<h1>Eave Lights</h1>
<div class="card"><button id="power" class="power">…</button></div>
<div class="card"><label>Color</label><input id="color" type="color"></div>
<div class="card"><label>Brightness <span id="briVal"></span></label><input id="bri" type="range" min="0" max="255"></div>
<div class="card"><label>Mode</label><div class="modes">
<button data-mode="solid">Solid</button><button data-mode="fade">Fade</button><button data-mode="test">Test</button>
</div></div>
<div class="card" id="fadeCard" hidden>
<label>Fade speed: <span id="fsecVal"></span> per breath</label><input id="fsec" type="range" min="2" max="60">
<label style="margin-top:14px">Fade lowest level: <span id="fminVal"></span></label><input id="fmin" type="range" min="0" max="230">
<p class="hint">Below about 20% the dim end may show steps.</p>
</div>
<p class="status" id="status">Connecting…</p>
</main><script>
const $=id=>document.getElementById(id);let st={};let timer=null;
function show(s){st=s;
 $('power').textContent=s.on?'ON':'OFF';$('power').classList.toggle('on',s.on);
 $('color').value=s.color.toLowerCase();$('bri').value=s.brightness;$('briVal').textContent=Math.round(s.brightness/2.55)+'%';
 document.querySelectorAll('.modes button').forEach(b=>b.classList.toggle('sel',b.dataset.mode===s.mode));
 $('fadeCard').hidden=s.mode!=='fade';
 $('fsec').value=s.fadeSeconds;$('fsecVal').textContent=s.fadeSeconds+' s';
 $('fmin').value=s.fadeMin;$('fminVal').textContent=Math.round(s.fadeMin/2.55)+'%';
 $('status').textContent='Connected · v'+s.version;}
async function send(p){try{const r=await fetch('/api/state',{method:'POST',body:new URLSearchParams(p)});show(await r.json());}
 catch(e){$('status').textContent='Not connected';}}
function sendSoon(p){clearTimeout(timer);timer=setTimeout(()=>send(p),120);}
$('power').onclick=()=>send({on:st.on?0:1});
$('color').oninput=e=>sendSoon({color:e.target.value.slice(1)});
$('bri').oninput=e=>{$('briVal').textContent=Math.round(e.target.value/2.55)+'%';sendSoon({brightness:e.target.value});};
document.querySelectorAll('.modes button').forEach(b=>b.onclick=()=>send({mode:b.dataset.mode}));
$('fsec').oninput=e=>{$('fsecVal').textContent=e.target.value+' s';sendSoon({fadeSeconds:e.target.value});};
$('fmin').oninput=e=>{$('fminVal').textContent=Math.round(e.target.value/2.55)+'%';sendSoon({fadeMin:e.target.value});};
fetch('/api/state').then(r=>r.json()).then(show).catch(()=>$('status').textContent='Not connected');
</script></body></html>)rawhtml";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void startWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/state", HTTP_POST, handleSetState);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println("Web: server started on port 80");
}

void handleWeb() {
  server.handleClient();
}
