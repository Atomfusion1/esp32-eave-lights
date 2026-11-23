// web_api — control page and JSON API on port 80.
//
//   GET  /            control page
//   GET  /api/state   {"version":..., "outputs":[{A}, {B}], "house":{...}}
//   POST /api/state   form-encoded body or URL query, any of:
//                       output=all|a|b|house  what to change (default all = both strands;
//                                             "all" also lines up their fades/movement).
//                                             house = Whole house path; while it is on it
//                                             covers both strands. Turning it on restarts
//                                             its effect from the first light (B65).
//                       on=0|1  color=RRGGBB  brightness=0-255
//                       mode=solid|fade|test|chase|comet
//                       fadeSeconds=2-60  fadeMin=0-255
//                       speed=1-100 (lights/s)  size=1-30  reverse=0|1
//                     Invalid values change nothing and return 400. Returns the new state.
//   GET  /api/frame   the exact colors being sent to the LEDs right now (after power caps),
//                     binary: [outputs][leds low byte][leds high byte] then R,G,B per LED,
//                     output A's LEDs first. The page polls this for the live view.
//
// No login: anyone on the same network can use it. Fine on a home network.

WebServer server(80);

const char* MODE_NAMES[MODE_COUNT] = { "solid", "fade", "test", "chase", "comet" };

// The settings fields shared by strands and paths, without braces.
String channelFieldsJson(int ch) {
  const LightState& s = states[ch];
  char buf[220];
  snprintf(buf, sizeof(buf),
           "\"on\":%s,\"color\":\"#%02X%02X%02X\",\"brightness\":%u,\"mode\":\"%s\","
           "\"fadeSeconds\":%u,\"fadeMin\":%u,\"speed\":%u,\"size\":%u,\"reverse\":%s",
           s.on ? "true" : "false", s.color.r, s.color.g, s.color.b, s.brightness,
           MODE_NAMES[s.mode], s.fadeSeconds, s.fadeMin, s.speed, s.size,
           s.reverse ? "true" : "false");
  return String(buf);
}

String stateJson() {
  String json = "{\"version\":\"" FW_VERSION "\",\"outputs\":[";
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    char buf[80];
    snprintf(buf, sizeof(buf), "%s{\"id\":\"%c\",\"gpio\":%d,\"enabled\":%s,",
             o ? "," : "", OUTPUT_LETTERS[o], OUTPUT_PINS[o],
             OUTPUT_ENABLED[o] ? "true" : "false");
    json += buf;
    json += channelFieldsJson(o);
    json += "}";
  }
  json += "],\"house\":{\"id\":\"house\",\"name\":\"Whole house\",\"length\":";
  json += HOUSE_PATH_LEN;
  json += ",";
  json += channelFieldsJson(CH_HOUSE);
  json += "}}";
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
  int target = -1;  // -1 = both strands, else a channel number
  if (server.hasArg("output")) {
    String v = server.arg("output");
    v.toLowerCase();
    if (v == "all")                        target = -1;
    else if (v == "a" || v == "0")         target = 0;
    else if (v == "b" || v == "1")         target = 1;
    else if (v == "house" || v == "path")  target = CH_HOUSE;
    else return sendError("output must be all, a, b or house");
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
    if (found < 0) return sendError("mode must be solid, fade, test, chase or comet");
    newMode = (Mode)found;
  }

  // ---- 2. Apply to the target channel(s) ----
  bool changed = false;
  for (int o = 0; o < NUM_CHANNELS; o++) {
    if (target < 0 && o >= NUM_OUTPUTS) continue;   // "all" = the strands only
    if (target >= 0 && o != target) continue;
    LightState& s = states[o];

    if (server.hasArg("on")) {
      String v = server.arg("on");
      bool wasOn = s.on;
      s.on = (v == "1" || v == "true" || v == "on");
      if (o == CH_HOUSE && s.on && !wasOn) resetPhase(o);  // start again from B65
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
    if (server.hasArg("speed")) {
      s.speed = (uint8_t)constrain(server.arg("speed").toInt(), SPEED_MIN, SPEED_MAX);
      changed = true;
    }
    if (server.hasArg("size")) {
      s.size = (uint8_t)constrain(server.arg("size").toInt(), SIZE_MIN, SIZE_MAX);
      changed = true;
    }
    if (server.hasArg("reverse")) {
      String v = server.arg("reverse");
      s.reverse = (v == "1" || v == "true");
      changed = true;
    }
  }

  // Changing both strands together: keep their fades and movement in step.
  if (target < 0 && (hasMode || server.hasArg("on") || server.hasArg("fadeSeconds") ||
                     server.hasArg("speed") || server.hasArg("size") || server.hasArg("reverse"))) {
    syncPhases();
  }

  if (changed) markStateChanged();
  server.send(200, "application/json", stateJson());
}

// Live view: copies out[] (what FastLED is sending, after caps) into one binary reply.
// out[] holds plain RGB; the RBG reorder for the strand happens later inside FastLED.
void handleGetFrame() {
  static uint8_t buf[3 + NUM_OUTPUTS * NUM_LEDS * 3];
  size_t n = 0;
  buf[n++] = NUM_OUTPUTS;
  buf[n++] = NUM_LEDS & 0xFF;
  buf[n++] = NUM_LEDS >> 8;
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      buf[n++] = out[o][i].r;
      buf[n++] = out[o][i].g;
      buf[n++] = out[o][i].b;
    }
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char*)buf, n);
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
.seg{display:flex;gap:8px;flex-wrap:wrap}
#modes button{flex:1 1 28%}
.seg button{flex:1;padding:12px;border-radius:10px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:15px}
.seg button.sel{border-color:var(--accent);background:var(--accent);color:#fff}
.seg button small{display:block;font-size:11px;opacity:.8}
#tabs button{padding:10px 6px;flex:1 1 40%}
.live{background:#0b0b0a;border-radius:10px;padding:8px 6px}
.liveRow{display:flex;align-items:center;gap:6px}
.liveRow canvas{flex:1;height:26px;min-width:0}
.liveLabels{display:flex;justify-content:space-between;color:#8a8a84;font-size:10px;padding:2px 2px 0}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;border:1px solid rgba(128,128,128,.5);vertical-align:-1px}
.status{color:var(--muted);font-size:13px;text-align:center}
.hint{color:var(--muted);font-size:12px;margin:8px 0 0}
</style></head><body><main>
<h1>Eave Lights</h1>
<div class="card">
<label>Control</label>
<div class="seg" id="tabs"></div>
<p class="hint" id="tabHint" hidden></p>
</div>
<div class="card"><label>Live view <span id="liveInfo"></span></label><div class="live" id="live"></div>
<p class="hint">What the board is sending, light by light, as seen from the street. Screen brightness is approximate.</p></div>
<div class="card"><button id="power" class="power">…</button></div>
<div class="card"><label>Color</label><input id="color" type="color"></div>
<div class="card"><label>Brightness <span id="briVal"></span></label><input id="bri" type="range" min="0" max="255"></div>
<div class="card"><label>Mode</label><div class="seg" id="modes">
<button data-mode="solid">Solid</button><button data-mode="fade">Fade</button><button data-mode="chase">Chase</button>
<button data-mode="comet">Comet</button><button data-mode="test">Test</button>
</div></div>
<div class="card" id="motionCard" hidden>
<label>Speed: <span id="spdVal"></span></label><input id="spd" type="range" min="1" max="100">
<label style="margin-top:14px"><span id="sizeName"></span>: <span id="sizeVal"></span></label><input id="size" type="range" min="1" max="30">
<label style="margin-top:14px">Direction</label>
<div class="seg" id="dir"><button data-rev="0">Light 1 → 65</button><button data-rev="1">Light 65 → 1</button></div>
</div>
<div class="card" id="fadeCard" hidden>
<label>Fade speed: <span id="fsecVal"></span> per breath</label><input id="fsec" type="range" min="2" max="60">
<label style="margin-top:14px">Fade lowest level: <span id="fminVal"></span></label><input id="fmin" type="range" min="0" max="230">
<p class="hint">Below about 20% the dim end may show steps.</p>
</div>
<p class="status" id="status">Connecting…</p>
</main><script>
const $=id=>document.getElementById(id);let st=null,timer=null;
// sel = what is being edited: 0/1 = strand A/B, BOTH = both strands, HOUSE = Whole house path
const BOTH=-1,HOUSE=-2;
let sel=0;try{const v=localStorage.getItem('eaveSel');if(v!==null)sel=+v;}catch(e){}
const pct=v=>Math.round(v/2.55)+'%';
const both=()=>sel===BOTH,house=()=>sel===HOUSE;
function cur(){return house()?st.house:st.outputs[both()?0:sel];}
function anyOn(){return st.outputs.some(o=>o.on);}
function pick(i){sel=i;try{localStorage.setItem('eaveSel',i);}catch(e){}show(st);}
function tab(i,dot,name,sub){return '<button data-i="'+i+'" class="'+(i===sel?'sel':'')+'">'+
 '<span class="dot" style="background:'+(dot||'transparent')+'"></span>'+name+'<small>'+sub+'</small></button>';}
function show(s){st=s;if(sel>=s.outputs.length||sel<HOUSE)sel=0;
 const h=s.house,hOn=h.on,over=hOn?' · covered':'';
 $('tabs').innerHTML=s.outputs.map((o,i)=>tab(i,o.on?o.color:'','Strand '+o.id,'GPIO '+o.gpio+' · '+(o.on?'on':'off')+over)).join('')+
  tab(BOTH,'','Both strands',(s.outputs.every(o=>o.on)?'all on':anyOn()?'some on':'all off')+over)+
  tab(HOUSE,hOn?h.color:'','Whole house',(h.reverse?'left → right':'right → left')+' · '+(hOn?'on':'off'));
 $('tabs').querySelectorAll('button').forEach(b=>b.onclick=()=>pick(+b.dataset.i));
 const o=cur();
 let hint='';
 if(house())hint='One '+h.length+'-light line across the front: B65 (far right) → controller → A65 (far left). While it is on it covers Strand A and B; turn it off to go back to them.';
 else if(hOn)hint='Whole house is on and covering the strands. These settings show again when it is turned off.';
 else if(!both()&&!o.enabled)hint='This strand is turned off in the firmware (ENABLE_OUTPUT_B).';
 $('tabHint').textContent=hint;$('tabHint').hidden=!hint;
 const isOn=both()?anyOn():o.on;
 $('power').textContent=(house()?'Whole house ':both()?'Both strands ':'Strand '+o.id+' ')+(isOn?'ON':'OFF');
 $('power').classList.toggle('on',isOn);
 // Facing the house: A runs left from the controller, B runs right.
 const [fwd,rev]=house()?['Right → left','Left → right']
  :o.id==='A'?['Light 1 → 65 (outward, left)','Light 65 → 1 (inward)']
  :o.id==='B'?['Light 1 → 65 (outward, right)','Light 65 → 1 (inward)']
  :['Outward from controller','Inward to controller'];
 $('dir').children[0].textContent=fwd;$('dir').children[1].textContent=rev;
 $('color').value=o.color.toLowerCase();$('bri').value=o.brightness;$('briVal').textContent=pct(o.brightness);
 $('modes').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',b.dataset.mode===o.mode));
 $('fadeCard').hidden=o.mode!=='fade';
 $('fsec').value=o.fadeSeconds;$('fsecVal').textContent=o.fadeSeconds+' s';
 $('fmin').value=o.fadeMin;$('fminVal').textContent=pct(o.fadeMin);
 const moving=o.mode==='chase'||o.mode==='comet';$('motionCard').hidden=!moving;
 $('spd').value=o.speed;$('spdVal').textContent=o.speed+' lights/s';
 $('sizeName').textContent=o.mode==='comet'?'Tail length':'Lit block length';
 $('size').value=o.size;$('sizeVal').textContent=o.size+(o.size===1?' light':' lights');
 $('dir').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',(b.dataset.rev==='1')===o.reverse));
 $('status').textContent='Connected · v'+s.version;}
async function send(p){if(!st)return;p.output=house()?'house':both()?'all':st.outputs[sel].id;
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
$('spd').oninput=e=>{$('spdVal').textContent=e.target.value+' lights/s';sendSoon({speed:e.target.value});};
$('size').oninput=e=>{$('sizeVal').textContent=e.target.value+(e.target.value==='1'?' light':' lights');sendSoon({size:e.target.value});};
$('dir').querySelectorAll('button').forEach(b=>b.onclick=()=>send({reverse:b.dataset.rev}));
fetch('/api/state').then(r=>r.json()).then(show).catch(()=>$('status').textContent='Not connected');

// ---- Live view: polls /api/frame, one request at a time, ~10 per second ----
// LED values are linear; the screen is gamma-encoded, so dim LED values are lifted to look similar.
const GAM=new Uint8Array(256).map((_,v)=>Math.round(255*Math.pow(v/255,1/2.2)));
let cv=null,frames=0,fpsT=performance.now();
// Drawn as seen from the street: strand A runs left from the controller (A65 far left),
// strand B runs right (B65 far right), light 1 of each next to the controller in the middle.
const GAP=3;  // empty slots for the controller mark
function slotOf(o,i,leds){return o===0?leds-1-i:leds+GAP+i;}
function drawFrame(b){const outs=b[0],leds=b[1]|(b[2]<<8);
 if(!cv){$('live').innerHTML='<div class="liveRow"><canvas></canvas></div>'+
  '<div class="liveLabels"><span>A65 · left</span><span>controller</span><span>right · B65</span></div>';
  cv=$('live').querySelector('canvas');}
 const dpr=window.devicePixelRatio||1,w=cv.clientWidth,h=cv.clientHeight;
 if(cv.width!==Math.round(w*dpr)){cv.width=Math.round(w*dpr);cv.height=Math.round(h*dpr);}
 const g=cv.getContext('2d');g.setTransform(dpr,0,0,dpr,0,0);g.clearRect(0,0,w,h);
 const slots=2*leds+GAP,step=w/slots,r=Math.max(1,Math.min(step*0.4,6));
 g.shadowBlur=0;g.fillStyle='#55554f';g.fillRect(step*(leds+GAP/2)-1,h*0.2,2,h*0.6);
 for(let o=0;o<Math.min(outs,2);o++)for(let i=0;i<leds;i++){const k=3+(o*leds+i)*3,R=b[k],G=b[k+1],B=b[k+2];
  const lit=R|G|B;g.fillStyle=lit?'rgb('+GAM[R]+','+GAM[G]+','+GAM[B]+')':'#2a2a27';
  g.shadowBlur=lit?r*3:0;g.shadowColor=g.fillStyle;
  g.beginPath();g.arc(step*(slotOf(o,i,leds)+0.5),h/2,r,0,Math.PI*2);g.fill();}}
async function pollFrame(){if(document.hidden){setTimeout(pollFrame,500);return;}
 const t0=performance.now();
 try{const r=await fetch('/api/frame',{cache:'no-store'});drawFrame(new Uint8Array(await r.arrayBuffer()));frames++;}catch(e){}
 const now=performance.now();if(now-fpsT>=2000){$('liveInfo').textContent='· '+(frames*1000/(now-fpsT)).toFixed(1)+' updates/s';frames=0;fpsT=now;}
 setTimeout(pollFrame,Math.max(0,100-(now-t0)));}
pollFrame();
</script></body></html>)rawhtml";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void startWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/state", HTTP_POST, handleSetState);
  server.on("/api/frame", HTTP_GET, handleGetFrame);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println("Web: server started on port 80");
}

void handleWeb() {
  server.handleClient();
}
