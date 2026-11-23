// web_api — control page and JSON API on port 80.
//
//   GET  /            control page
//   GET  /api/state   {"version":..., "outputs":[{...}, {...}]}
//   POST /api/state   form-encoded body or URL query, any of:
//                       output=all|a|b|0|1   which strand to change (default all).
//                                            "all" also lines up the fades.
//                       on=0|1  color=RRGGBB  brightness=0-255  mode=solid|fade|test
//                       fadeSeconds=2-60  fadeMin=0-255
//                     Invalid values change nothing and return 400. Returns the new state.
//
// No login: anyone on the same network can use it. Fine on a home network.

WebServer server(80);

const char* MODE_NAMES[MODE_COUNT] = { "solid", "fade", "test" };

String stateJson() {
  String json = "{\"version\":\"" FW_VERSION "\",\"outputs\":[";
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    const LightState& s = states[o];
    char buf[256];
    snprintf(buf, sizeof(buf),
             "%s{\"id\":\"%c\",\"gpio\":%d,\"enabled\":%s,\"on\":%s,"
             "\"color\":\"#%02X%02X%02X\",\"brightness\":%u,\"mode\":\"%s\","
             "\"fadeSeconds\":%u,\"fadeMin\":%u}",
             o ? "," : "", OUTPUT_LETTERS[o], OUTPUT_PINS[o],
             OUTPUT_ENABLED[o] ? "true" : "false", s.on ? "true" : "false",
             s.color.r, s.color.g, s.color.b, s.brightness, MODE_NAMES[s.mode],
             s.fadeSeconds, s.fadeMin);
    json += buf;
  }
  json += "]}";
  return json;
}

void sendError(const char* message) {
  String body = "{\"error\":\"";
  body += message;
  body += "\"}";
  server.send(400, "application/json", body);
}

void handleGetState() {
  server.send(200, "application/json", stateJson());
}

void handleSetState() {
  // ---- 1. Parse and validate everything first, so a bad value changes nothing ----
  int target = -1;  // -1 = all outputs
  if (server.hasArg("output")) {
    String v = server.arg("output");
    v.toLowerCase();
    if (v == "all")                 target = -1;
    else if (v == "a" || v == "0")  target = 0;
    else if (v == "b" || v == "1")  target = 1;
    else return sendError("output must be all, a or b");
  }

  bool hasColor = server.hasArg("color");
  CRGB newColor;
  if (hasColor) {
    String v = server.arg("color");
    if (v.startsWith("#")) v = v.substring(1);
    if (v.length() != 6) return sendError("color must be RRGGBB");
    newColor = CRGB((uint32_t)strtoul(v.c_str(), nullptr, 16));
  }

  bool hasMode = server.hasArg("mode");
  Mode newMode = MODE_SOLID;
  if (hasMode) {
    String v = server.arg("mode");
    int found = -1;
    for (int i = 0; i < MODE_COUNT; i++) {
      if (v == MODE_NAMES[i]) found = i;
    }
    if (found < 0) return sendError("mode must be solid, fade or test");
    newMode = (Mode)found;
  }

  // ---- 2. Apply to the target output(s) ----
  bool changed = false;
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    if (target >= 0 && o != target) continue;
    LightState& s = states[o];

    if (server.hasArg("on")) {
      String v = server.arg("on");
      s.on = (v == "1" || v == "true" || v == "on");
      changed = true;
    }
    if (hasColor) { s.color = newColor; changed = true; }
    if (server.hasArg("brightness")) {
      s.brightness = (uint8_t)constrain(server.arg("brightness").toInt(), 0, 255);
      changed = true;
    }
    if (hasMode) { s.mode = newMode; changed = true; }
    if (server.hasArg("fadeSeconds")) {
      s.fadeSeconds = (uint8_t)constrain(server.arg("fadeSeconds").toInt(),
                                         FADE_SECONDS_MIN, FADE_SECONDS_MAX);
      changed = true;
    }
    if (server.hasArg("fadeMin")) {
      s.fadeMin = (uint8_t)constrain(server.arg("fadeMin").toInt(), 0, 255);
      changed = true;
    }
  }

  // Changing both strands together: keep their fades in step.
  if (target < 0 && (hasMode || server.hasArg("fadeSeconds") || server.hasArg("on"))) {
    syncFadePhases();
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
.seg{display:flex;gap:8px}
.seg button{flex:1;padding:12px;border-radius:10px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:15px}
.seg button.sel{border-color:var(--accent);background:var(--accent);color:#fff}
.seg button small{display:block;font-size:11px;opacity:.8}
#tabs button{padding:10px 6px}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;border:1px solid rgba(128,128,128,.5);vertical-align:-1px}
.status{color:var(--muted);font-size:13px;text-align:center}
.hint{color:var(--muted);font-size:12px;margin:8px 0 0}
</style></head><body><main>
<h1>Eave Lights</h1>
<div class="card">
<label>Strand</label>
<div class="seg" id="tabs"></div>
<p class="hint" id="disabledHint" hidden>This strand is turned off in the firmware (ENABLE_OUTPUT_B).</p>
</div>
<div class="card"><button id="power" class="power">…</button></div>
<div class="card"><label>Color</label><input id="color" type="color"></div>
<div class="card"><label>Brightness <span id="briVal"></span></label><input id="bri" type="range" min="0" max="255"></div>
<div class="card"><label>Mode</label><div class="seg" id="modes">
<button data-mode="solid">Solid</button><button data-mode="fade">Fade</button><button data-mode="test">Test</button>
</div></div>
<div class="card" id="fadeCard" hidden>
<label>Fade speed: <span id="fsecVal"></span> per breath</label><input id="fsec" type="range" min="2" max="60">
<label style="margin-top:14px">Fade lowest level: <span id="fminVal"></span></label><input id="fmin" type="range" min="0" max="230">
<p class="hint">Below about 20% the dim end may show steps.</p>
</div>
<p class="status" id="status">Connecting…</p>
</main><script>
const $=id=>document.getElementById(id);let st=null,timer=null;
// sel = index of the strand being edited, or -1 for both
let sel=0;try{const v=localStorage.getItem('eaveSel');if(v!==null)sel=+v;}catch(e){}
const pct=v=>Math.round(v/2.55)+'%';
const both=()=>sel<0;
function cur(){return st.outputs[both()?0:sel];}
function anyOn(){return st.outputs.some(o=>o.on);}
function pick(i){sel=i;try{localStorage.setItem('eaveSel',i);}catch(e){}show(st);}
function show(s){st=s;if(sel>=s.outputs.length)sel=0;
 $('tabs').innerHTML=s.outputs.map((o,i)=>'<button data-i="'+i+'" class="'+(i===sel?'sel':'')+'">'+
  '<span class="dot" style="background:'+(o.on?o.color:'transparent')+'"></span>Strand '+o.id+
  '<small>GPIO '+o.gpio+' · '+(o.on?'on':'off')+'</small></button>').join('')+
  '<button data-i="-1" class="'+(both()?'sel':'')+'">Both<small>'+(s.outputs.every(o=>o.on)?'all on':anyOn()?'some on':'all off')+'</small></button>';
 $('tabs').querySelectorAll('button').forEach(b=>b.onclick=()=>pick(+b.dataset.i));
 const o=cur();
 $('disabledHint').hidden=both()||o.enabled;
 const isOn=both()?anyOn():o.on;
 $('power').textContent=(both()?'Both strands ':'Strand '+o.id+' ')+(isOn?'ON':'OFF');
 $('power').classList.toggle('on',isOn);
 $('color').value=o.color.toLowerCase();$('bri').value=o.brightness;$('briVal').textContent=pct(o.brightness);
 $('modes').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',b.dataset.mode===o.mode));
 $('fadeCard').hidden=o.mode!=='fade';
 $('fsec').value=o.fadeSeconds;$('fsecVal').textContent=o.fadeSeconds+' s';
 $('fmin').value=o.fadeMin;$('fminVal').textContent=pct(o.fadeMin);
 $('status').textContent='Connected · v'+s.version;}
async function send(p){if(!st)return;p.output=both()?'all':st.outputs[sel].id;
 try{const r=await fetch('/api/state',{method:'POST',body:new URLSearchParams(p)});const j=await r.json();
  if(j.error){$('status').textContent=j.error;}else show(j);}
 catch(e){$('status').textContent='Not connected';}}
function sendSoon(p){clearTimeout(timer);timer=setTimeout(()=>send(p),120);}
// Both: if any strand is on, turn all off; otherwise turn all on.
$('power').onclick=()=>send({on:(both()?anyOn():cur().on)?0:1});
$('color').oninput=e=>sendSoon({color:e.target.value.slice(1)});
$('bri').oninput=e=>{$('briVal').textContent=pct(e.target.value);sendSoon({brightness:e.target.value});};
$('modes').querySelectorAll('button').forEach(b=>b.onclick=()=>send({mode:b.dataset.mode}));
$('fsec').oninput=e=>{$('fsecVal').textContent=e.target.value+' s';sendSoon({fadeSeconds:e.target.value});};
$('fmin').oninput=e=>{$('fminVal').textContent=pct(e.target.value);sendSoon({fadeMin:e.target.value});};
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
