// web_page.h — the control page served at "/". Plain HTML/CSS/JS, talks to web_api.ino.
//
// This is a .h on purpose: the Arduino IDE scans .ino tabs to declare functions and
// mistakes JavaScript "function name(){" lines for C++, breaking the build.
//
// Sections: what to control (strands / Both / Whole house / zones), live view, power,
// saved looks, color, brightness, mode + mode settings, and Setup (Find a light, zones).

#pragma once

const char INDEX_HTML[] PROGMEM = R"rawhtml(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Eave Lights</title>
<style>
:root{--bg:#f4f4f2;--card:#fff;--text:#1d1d1b;--muted:#6b6b66;--accent:#d9822b;--line:#deded9;--bad:#c0392b}
@media (prefers-color-scheme:dark){:root{--bg:#141413;--card:#1f1f1d;--text:#ececea;--muted:#9a9a94;--accent:#e8954a;--line:#33332f;--bad:#e06656}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,sans-serif}
main{max-width:460px;margin:0 auto;padding:20px 16px}
h1{font-size:22px;margin:0 0 16px}
h2{font-size:15px;margin:0 0 10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:14px}
label{display:block;color:var(--muted);font-size:13px;margin-bottom:8px}
button{font:inherit}
button.power{width:100%;padding:16px;font-size:18px;font-weight:600;border-radius:12px;border:none;cursor:pointer;background:var(--line);color:var(--text)}
button.power.on{background:var(--accent);color:#fff}
input[type=color]{width:100%;height:56px;border:none;border-radius:10px;background:none;padding:0}
input[type=range]{width:100%;accent-color:var(--accent)}
input[type=text],input[type=number]{width:100%;padding:10px;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--text);font-size:16px}
.seg{display:flex;gap:8px;flex-wrap:wrap}
.seg button{flex:1;padding:12px;border-radius:10px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:15px}
.seg button.sel{border-color:var(--accent);background:var(--accent);color:#fff}
.seg button small{display:block;font-size:11px;opacity:.8}
#modes button{flex:1 1 28%}
#tabs button{padding:10px 6px;flex:1 1 40%}
.btn{padding:10px 14px;border-radius:10px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:15px}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.btn.danger{color:var(--bad)}
.row{display:flex;gap:8px;align-items:center}
.row>*{flex:1}
.row .btn{flex:0 0 auto}
.chips{display:flex;flex-wrap:wrap;gap:8px}
.chip{display:inline-flex;align-items:center;gap:6px;padding:8px 10px;border-radius:20px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:14px}
.chip .x{margin-left:2px;padding:0 4px;color:var(--muted);border-radius:8px}
.chip .x:hover{color:var(--bad)}
.live{background:#0b0b0a;border-radius:10px;padding:8px 6px}
.liveRow{display:flex;align-items:center}
.liveRow canvas{flex:1;height:26px;min-width:0}
.liveLabels{display:flex;justify-content:space-between;color:#8a8a84;font-size:10px;padding:2px 2px 0}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;border:1px solid rgba(128,128,128,.5);vertical-align:-1px}
.chip .dot{margin:0}
.status{color:var(--muted);font-size:13px;text-align:center}
.hint{color:var(--muted);font-size:12px;margin:8px 0 0}
.err{color:var(--bad);font-size:13px;margin:8px 0 0}
details summary{cursor:pointer;font-weight:600;font-size:15px}
details[open] summary{margin-bottom:14px}
.zoneRow{display:flex;align-items:center;gap:8px;padding:8px 0;border-top:1px solid var(--line)}
.zoneRow:first-child{border-top:none}
.zoneRow div{flex:1;min-width:0}
.zoneRow small{display:block;color:var(--muted)}
.sub{border-top:1px solid var(--line);margin-top:16px;padding-top:14px}
</style></head><body><main>
<h1>Eave Lights</h1>

<div class="card">
<label>Control</label>
<div class="seg" id="tabs"></div>
<p class="hint" id="tabHint" hidden></p>
</div>

<div class="card"><label>Live view <span id="liveInfo"></span></label><div class="live" id="live"></div>
<p class="hint">What the board is sending, light by light, as seen from the street. Screen brightness is approximate.</p>
<p class="err" id="locateBanner" hidden></p></div>

<div class="card"><button id="power" class="power">…</button></div>

<div class="card">
<label>Saved looks <span id="lookTarget"></span></label>
<div class="chips" id="looks"></div>
<p class="hint" id="looksEmpty">No saved looks yet. Set up a look below, then save it here.</p>
<div class="row" style="margin-top:12px"><button class="btn" id="saveLook">Save current look…</button></div>
<p class="err" id="lookErr" hidden></p>
</div>

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
<div class="seg" id="dir"><button data-rev="0"></button><button data-rev="1"></button></div>
<label style="margin-top:14px">Background (lights between / behind the effect)</label>
<div class="seg" id="bg"><button data-bg="0">Off</button><button data-bg="1">On</button></div>
<input id="bgColor" type="color" style="margin-top:10px" hidden>
<p class="hint" id="bgHint" hidden>Uses the same brightness. Lights every light, so it adds current.</p>
</div>
<div class="card" id="fadeCard" hidden>
<label>Fade speed: <span id="fsecVal"></span> per breath</label><input id="fsec" type="range" min="2" max="60">
<label style="margin-top:14px">Fade lowest level: <span id="fminVal"></span></label><input id="fmin" type="range" min="0" max="230">
<p class="hint">Below about 20% the dim end may show steps.</p>
</div>

<div class="card"><details id="setup"><summary>Setup: find lights &amp; zones</summary>
<h2>Find a light</h2>
<p class="hint" style="margin:0 0 10px">Lights only the chosen light (green) so you can see where it is. Turns itself off after 5 minutes.</p>
<div class="seg" id="findStrand"><button data-s="A">Strand A (left)</button><button data-s="B">Strand B (right)</button></div>
<div class="row" style="margin-top:10px">
<button class="btn" id="findMinus">−</button><input id="findNum" type="number" min="1" max="65" value="1"><button class="btn" id="findPlus">+</button>
</div>
<div class="row" style="margin-top:10px"><button class="btn primary" id="findShow">Show light</button><button class="btn" id="findStop">Stop</button></div>

<div class="sub">
<h2>Zones</h2>
<p class="hint" style="margin:0 0 10px">A zone is a name plus lights, e.g. <b>A1-12</b> or <b>A1-12, B30-65</b>. <b>A12-1</b> runs backwards. Zones that are on are drawn on top of the strands and Whole house.</p>
<div id="zoneList"></div>
<p class="hint" id="noZones">No zones yet.</p>
<div class="sub">
<label id="zoneFormTitle">New zone</label>
<input id="zoneName" type="text" placeholder="Name, e.g. Porch" maxlength="23">
<input id="zoneRanges" type="text" placeholder="Lights, e.g. A1-12, B30-65" style="margin-top:8px">
<div class="row" style="margin-top:10px">
<button class="btn" id="zoneShow">Show on lights</button><button class="btn primary" id="zoneSave">Save zone</button><button class="btn" id="zoneCancel" hidden>Cancel</button>
</div>
<p class="err" id="zoneErr" hidden></p>
</div>
</div>
<p class="err" id="storageWarn" hidden>Storage not found, so zones and saved looks are lost on restart. Use Tools → Partition Scheme → 16M Flash (3MB APP/9.9MB FATFS).</p>
</details></div>

<p class="status" id="status">Connecting…</p>
</main><script>
const $=id=>document.getElementById(id);let st=null,timer=null,looks=[],editZone=-1,findStrand='A';
// sel = what the controls change: 0/1 strand A/B, BOTH, HOUSE, or ZONE+slot
const BOTH=-1,HOUSE=-2,ZONE=100;
let sel=0;try{const v=localStorage.getItem('eaveSel');if(v!==null)sel=+v;}catch(e){}
const pct=v=>Math.round(v/2.55)+'%';
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const both=()=>sel===BOTH,house=()=>sel===HOUSE,zoneSel=()=>sel>=ZONE;
const zoneBySlot=n=>st.zones.find(z=>z.slot===n);
function cur(){return house()?st.house:zoneSel()?zoneBySlot(sel-ZONE):st.outputs[both()?0:sel];}
function target(){return house()?'house':both()?'all':zoneSel()?'z'+(sel-ZONE):st.outputs[sel].id;}
function curName(){return house()?'Whole house':both()?'Both strands':zoneSel()?cur().name:'Strand '+cur().id;}
function anyOn(){return st.outputs.some(o=>o.on);}
function pick(i){sel=i;try{localStorage.setItem('eaveSel',i);}catch(e){}show(st);}
function tab(i,dot,name,sub){return '<button data-i="'+i+'" class="'+(i===sel?'sel':'')+'">'+
 '<span class="dot" style="background:'+(dot||'transparent')+'"></span>'+esc(name)+'<small>'+esc(sub)+'</small></button>';}
function showErr(id,msg){$(id).textContent=msg||'';$(id).hidden=!msg;}

function show(s){st=s;
 if((zoneSel()&&!zoneBySlot(sel-ZONE))||(sel>=0&&sel<ZONE&&sel>=s.outputs.length)||(sel<HOUSE))sel=0;
 const h=s.house,zonesOn=s.zones.filter(z=>z.on);
 const coverStrands=(h.on?['Whole house']:[]).concat(zonesOn.map(z=>z.name));
 const over=h.on?' · covered':'';
 $('tabs').innerHTML=s.outputs.map((o,i)=>tab(i,o.on?o.color:'','Strand '+o.id,'GPIO '+o.gpio+' · '+(o.on?'on':'off')+over)).join('')+
  tab(BOTH,'','Both strands',(s.outputs.every(o=>o.on)?'all on':anyOn()?'some on':'all off')+over)+
  tab(HOUSE,h.on?h.color:'','Whole house',(h.reverse?'left → right':'right → left')+' · '+(h.on?'on':'off'))+
  s.zones.map(z=>tab(ZONE+z.slot,z.on?z.color:'',z.name,z.ranges+' · '+(z.on?'on':'off'))).join('');
 $('tabs').querySelectorAll('button').forEach(b=>b.onclick=()=>pick(+b.dataset.i));
 const o=cur();
 let hint='';
 if(house())hint='One '+h.length+'-light line across the front: B65 (far right) → controller → A65 (far left). While on, it covers Strand A and B'+(zonesOn.length?'; zones that are on ('+zonesOn.map(z=>z.name).join(', ')+') are drawn on top':'')+'.';
 else if(zoneSel())hint=o.length+' lights: '+o.ranges+'. While on, drawn on top of everything else.';
 else if(coverStrands.length)hint='Drawn on top of the strands right now: '+coverStrands.join(', ')+'.';
 else if(!both()&&!o.enabled)hint='This strand is turned off in the firmware (ENABLE_OUTPUT_B).';
 $('tabHint').textContent=hint;$('tabHint').hidden=!hint;
 const isOn=both()?anyOn():o.on;
 $('power').textContent=curName()+' '+(isOn?'ON':'OFF');
 $('power').classList.toggle('on',isOn);
 $('lookTarget').textContent='· tap to apply to '+curName();
 const [fwd,rev]=house()?['Right → left','Left → right']
  :zoneSel()?['As listed ('+o.ranges+')','Reversed']
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
 $('bg').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',(b.dataset.bg==='1')===o.bgOn));
 $('bgColor').hidden=!o.bgOn;$('bgHint').hidden=!o.bgOn;$('bgColor').value=o.bgColor.toLowerCase();
 // setup
 $('zoneList').innerHTML=s.zones.map(z=>'<div class="zoneRow"><div>'+esc(z.name)+'<small>'+esc(z.ranges)+' · '+z.length+' lights</small></div>'+
  '<button class="btn" data-edit="'+z.slot+'">Edit</button><button class="btn danger" data-del="'+z.slot+'">Delete</button></div>').join('');
 $('noZones').hidden=s.zones.length>0;
 $('zoneList').querySelectorAll('[data-edit]').forEach(b=>b.onclick=()=>startEdit(+b.dataset.edit));
 $('zoneList').querySelectorAll('[data-del]').forEach(b=>b.onclick=()=>delZone(+b.dataset.del));
 $('storageWarn').hidden=s.storage;
 $('findStrand').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',b.dataset.s===findStrand));
 $('locateBanner').hidden=!s.locate.active;
 $('locateBanner').textContent=s.locate.active?'Find a light is on: showing only '+s.locate.ranges+'. Press Stop in Setup to go back.':'';
 $('status').textContent='Connected · v'+s.version;}

async function post(url,p){const r=await fetch(url,{method:'POST',body:new URLSearchParams(p)});return r.json();}
async function send(p){if(!st)return;p.output=target();
 try{const j=await post('/api/state',p);if(j.error){$('status').textContent=j.error;}else show(j);}
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
$('bg').querySelectorAll('button').forEach(b=>b.onclick=()=>send({bgOn:b.dataset.bg}));
$('bgColor').oninput=e=>sendSoon({bgColor:e.target.value.slice(1)});

// ---- Saved looks ----
function showLooks(list){looks=list;
 $('looks').innerHTML=list.map(p=>'<span class="chip" data-id="'+p.id+'" title="'+esc(p.mode)+'">'+
  '<span class="dot" style="background:'+p.color+'"></span>'+(p.bgOn?'<span class="dot" style="background:'+p.bgColor+'"></span>':'')+
  esc(p.name)+'<span class="x" data-x="'+p.id+'" title="Delete">×</span></span>').join('');
 $('looksEmpty').hidden=list.length>0;
 $('looks').querySelectorAll('.chip').forEach(c=>c.onclick=async e=>{
  const x=e.target.dataset.x;
  if(x!==undefined){const p=looks.find(l=>l.id===+x);if(!confirm('Delete saved look "'+p.name+'"?'))return;
   const j=await post('/api/presets',{action:'delete',id:x});if(j.error)showErr('lookErr',j.error);else{showErr('lookErr');showLooks(j.presets);}return;}
  const j=await post('/api/presets',{action:'apply',id:c.dataset.id,output:target()});
  if(j.error)showErr('lookErr',j.error);else{showErr('lookErr');show(j);}});}
$('saveLook').onclick=async()=>{const name=prompt('Name for this look (e.g. Xmas, Valentines). Saves the current settings of '+curName()+'. Same name replaces it.');
 if(!name)return;const j=await post('/api/presets',{action:'save',name,output:target()});
 if(j.error)showErr('lookErr',j.error);else{showErr('lookErr');showLooks(j.presets);}};

// ---- Find a light ----
function findNum(){return Math.min(65,Math.max(1,parseInt($('findNum').value)||1));}
async function locate(ranges){const j=await post('/api/locate',ranges?{ranges}:{stop:1});if(j.error)$('status').textContent=j.error;else show(j);return j;}
function findShow(){$('findNum').value=findNum();locate(findStrand+findNum());}
$('findStrand').querySelectorAll('button').forEach(b=>b.onclick=()=>{findStrand=b.dataset.s;show(st);if(st.locate.active)findShow();});
$('findMinus').onclick=()=>{$('findNum').value=Math.max(1,findNum()-1);findShow();};
$('findPlus').onclick=()=>{$('findNum').value=Math.min(65,findNum()+1);findShow();};
$('findNum').onchange=findShow;
$('findShow').onclick=findShow;
$('findStop').onclick=()=>locate('');

// ---- Zones ----
function resetZoneForm(){editZone=-1;$('zoneName').value='';$('zoneRanges').value='';$('zoneFormTitle').textContent='New zone';$('zoneCancel').hidden=true;showErr('zoneErr');}
function startEdit(slot){const z=zoneBySlot(slot);editZone=slot;$('zoneName').value=z.name;$('zoneRanges').value=z.ranges;
 $('zoneFormTitle').textContent='Edit zone: '+z.name;$('zoneCancel').hidden=false;showErr('zoneErr');$('zoneName').focus();}
async function delZone(slot){const z=zoneBySlot(slot);if(!confirm('Delete zone "'+z.name+'"?'))return;
 const j=await post('/api/zones',{action:'delete',id:slot});if(j.error)showErr('zoneErr',j.error);else{if(editZone===slot)resetZoneForm();show(j);}}
$('zoneShow').onclick=async()=>{const j=await locate($('zoneRanges').value.trim());showErr('zoneErr',j.error);};
$('zoneSave').onclick=async()=>{const p={action:'save',name:$('zoneName').value,ranges:$('zoneRanges').value};if(editZone>=0)p.id=editZone;
 const j=await post('/api/zones',p);if(j.error){showErr('zoneErr',j.error);return;}
 if(j.locate.active)await locate('');resetZoneForm();show(j);};
$('zoneCancel').onclick=resetZoneForm;

fetch('/api/state').then(r=>r.json()).then(show).catch(()=>$('status').textContent='Not connected');
fetch('/api/presets').then(r=>r.json()).then(j=>showLooks(j.presets)).catch(()=>{});

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
