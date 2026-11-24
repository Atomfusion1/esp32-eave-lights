// web_page.h — the control page served at "/". Plain HTML/CSS/JS, talks to web_api.ino.
//
// This is a .h on purpose: the Arduino IDE scans .ino tabs to declare functions and
// mistakes JavaScript "function name(){" lines for C++, breaking the build.
//
// Four tabs:
//   Home   live view (house photo or dot strip), scenes, what's on, All lights off
//   Lights pick a strand / Whole house / path / zone (grouped drop-down), power,
//          saved looks, color + brightness + mode, mode settings in a fold-out
//   House  the photo editor (/editor?embed=1 in a frame)
//   Setup  fold-out sections: strand lengths, find a light, zones, paths,
//          backup & restore, firmware update
// The live view card moves between Home and Lights and only polls on those tabs.

#pragma once

const char INDEX_HTML[] PROGMEM = R"rawhtml(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Eave Lights</title>
<style>
:root{--bg:#f4f4f2;--card:#fff;--text:#1d1d1b;--muted:#6b6b66;--accent:#d9822b;--line:#deded9;--bad:#c0392b}
@media (prefers-color-scheme:dark){:root{--bg:#141413;--card:#1f1f1d;--text:#ececea;--muted:#9a9a94;--accent:#e8954a;--line:#33332f;--bad:#e06656}}
*{box-sizing:border-box}
[hidden]{display:none!important}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,sans-serif}
button.power:disabled{opacity:.45;cursor:default}
main{max-width:760px;margin:0 auto;padding:0 16px 24px}
body.wide main{max-width:1240px}
.top{position:sticky;top:0;z-index:5;background:var(--bg);padding:14px 0 10px;margin-bottom:6px;border-bottom:1px solid var(--line)}
.titleRow{display:flex;align-items:center;gap:10px;margin-bottom:10px}
.titleRow h1{font-size:20px;margin:0;flex:1}
.pill{font-size:12px;color:var(--muted);border:1px solid var(--line);border-radius:20px;padding:3px 10px;white-space:nowrap}
.pill.on{color:#fff;background:var(--accent);border-color:var(--accent)}
.tabs{display:flex;gap:6px}
.tabs button{flex:1;padding:10px 6px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--text);cursor:pointer;font:inherit;font-size:15px;font-weight:600}
.tabs button.sel{background:var(--accent);border-color:var(--accent);color:#fff}
section[data-tab]{display:none}
section[data-tab].show{display:block}
h2{font-size:15px;margin:0 0 10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin:14px 0}
label{display:block;color:var(--muted);font-size:13px;margin-bottom:8px}
button{font:inherit}
button.power{width:100%;padding:16px;font-size:18px;font-weight:600;border-radius:12px;border:none;cursor:pointer;background:var(--line);color:var(--text)}
button.power.on{background:var(--accent);color:#fff}
input[type=color]{width:100%;height:48px;border:none;border-radius:10px;background:none;padding:0}
input[type=range]{width:100%;accent-color:var(--accent)}
input[type=text],input[type=number],input[type=password],select{width:100%;padding:10px;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--text);font-size:16px}
select.picker{font-size:17px;font-weight:600;padding:12px}
.seg{display:flex;gap:8px;flex-wrap:wrap}
.seg button{flex:1;padding:11px;border-radius:10px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:15px}
.seg button.sel{border-color:var(--accent);background:var(--accent);color:#fff}
.seg.small button{flex:0 0 auto;padding:5px 10px;font-size:13px;border-radius:8px}
.applyBar{position:sticky;bottom:12px;z-index:4;display:flex;align-items:center;gap:8px;background:var(--card);border:2px solid var(--accent);border-radius:14px;padding:10px 12px;margin:14px 0;box-shadow:0 4px 18px rgba(0,0,0,.25)}
.applyBar span{flex:1;font-size:14px;font-weight:600}
.colorRow{display:flex;gap:8px}
.colorRow input[type=color]{flex:1;min-width:0;height:44px}
.btn{padding:10px 14px;border-radius:10px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:15px}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.btn.danger{color:var(--bad)}
.btn:disabled{opacity:.45;cursor:default}
.row{display:flex;gap:8px;align-items:center}
.row>*{flex:1}
.row .btn{flex:0 0 auto}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:14px}
@media (max-width:520px){.grid2{grid-template-columns:1fr}}
.chips{display:flex;flex-wrap:wrap;gap:8px}
.chip{display:inline-flex;align-items:center;gap:6px;padding:8px 12px;border-radius:20px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:14px}
.chip .x{margin-left:2px;padding:0 4px;color:var(--muted);border-radius:8px}
.chip .x:hover{color:var(--bad)}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;border:1px solid rgba(128,128,128,.5)}
.liveHead{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-bottom:10px}
.liveHead label{margin:0;flex:1}
.live{background:#0b0b0a;border-radius:10px;padding:8px 6px}
.liveRow{display:flex;align-items:center}
.liveRow canvas{flex:1;height:26px;min-width:0}
.liveLabels{display:flex;justify-content:space-between;color:#8a8a84;font-size:10px;padding:2px 2px 0}
.photo{background:#000;border-radius:10px;overflow:hidden}
.photo canvas{display:block;width:100%}
.status{color:var(--muted);font-size:13px;text-align:center;margin-top:14px}
.hint{color:var(--muted);font-size:12px;margin:8px 0 0}
.err{color:var(--bad);font-size:13px;margin:8px 0 0}
details.fold{background:var(--card);border:1px solid var(--line);border-radius:14px;margin:14px 0}
details.fold>summary{cursor:pointer;padding:14px 16px;font-weight:600;font-size:15px;list-style:none;display:flex;align-items:center;gap:8px}
details.fold>summary::-webkit-details-marker{display:none}
details.fold>summary::before{content:'▸';color:var(--muted);transition:transform .15s}
details.fold[open]>summary::before{transform:rotate(90deg)}
details.fold>summary small{font-weight:400;color:var(--muted);margin-left:auto;font-size:12px;text-align:right}
details.fold>.body{padding:0 16px 16px}
details.inner{border-top:1px solid var(--line);margin-top:14px;padding-top:10px}
details.inner>summary{cursor:pointer;color:var(--text);font-size:14px;font-weight:600;padding:4px 0}
.zoneRow{display:flex;align-items:center;gap:8px;padding:8px 6px;border-top:1px solid var(--line);border-radius:8px}
.zoneRow:first-child{border-top:none}
.zoneRow div{flex:1;min-width:0}
.zoneRow small{display:block;color:var(--muted)}
.zoneRow.click{cursor:pointer}
.zoneRow.click:hover{background:var(--bg)}
.zoneRow.showing{background:var(--accent);color:#fff}
.zoneRow.showing small{color:#fff}
.sub{border-top:1px solid var(--line);margin-top:16px;padding-top:14px}
.stepRow{display:flex;gap:6px;align-items:center;margin-top:6px}
.stepRow select{flex:1;min-width:0;padding:9px;font-size:15px}
.stepRow input{width:58px;flex:0 0 58px;padding:9px 6px}
.stepRow .btn{padding:9px 10px}
.stepHead{display:flex;gap:6px;color:var(--muted);font-size:11px;margin-top:8px}
.stepHead span:first-child{flex:1}
iframe.editor{width:100%;border:none;min-height:600px;display:block}
.onList{font-size:14px;margin:0}
</style></head><body><main>

<div class="top">
<div class="titleRow"><h1>Eave Lights</h1><span class="pill" id="onPill">…</span></div>
<div class="tabs" id="tabs">
<button data-t="home">Home</button><button data-t="lights">Lights</button><button data-t="house">House</button><button data-t="setup">Setup</button>
</div>
</div>

<!-- ===== Live view (moved into Home or Lights) ===== -->
<div class="card" id="liveCard">
<div class="liveHead">
<label>Live <span id="liveInfo"></span></label>
<div class="seg small" id="photoViews"></div>
<div class="seg small" id="liveMode"><button data-lm="photo">House</button><button data-lm="strip">Strip</button></div>
</div>
<div class="photo" id="photoBox"><canvas id="photoCv"></canvas></div>
<div class="live" id="stripBox"></div>
<p class="hint" id="liveHint"></p>
<p class="err" id="locateBanner" hidden></p>
</div>

<!-- ===== Home ===== -->
<section data-tab="home">
<div id="liveSlotHome"></div>
<div class="card">
<h2>Scenes</h2>
<div class="chips" id="scenes"></div>
<p class="hint" id="scenesEmpty">No scenes yet. Set up the whole house in the Lights tab, then save it here as a scene, e.g. "Xmas".</p>
<div class="row" style="margin-top:12px"><button class="btn" id="saveScene">Save current scene…</button></div>
<p class="err" id="sceneErr" hidden></p>
</div>
<div class="card">
<h2>On right now</h2>
<p class="onList" id="onList">…</p>
<button class="power" id="allOff" style="margin-top:12px">All lights off</button>
</div>
</section>

<!-- ===== Lights ===== -->
<section data-tab="lights">
<div class="card">
<div class="row" style="margin-bottom:10px;align-items:center;flex-wrap:wrap">
<label style="margin:0;flex:1">Controlling</label>
<span class="hint" style="margin:0;flex:0 0 auto">Changes go live:</span>
<div class="seg small" id="applyMode" style="flex:0 0 auto"><button data-am="apply">on Apply</button><button data-am="instant">instantly</button></div>
</div>
<select class="picker" id="picker"></select>
<p class="hint" id="tabHint" hidden></p>
<button id="power" class="power" style="margin-top:12px">…</button>
</div>
<div id="liveSlotLights"></div>
<div class="card">
<label>Saved looks <span id="lookTarget"></span></label>
<div class="chips" id="looks"></div>
<p class="hint" id="looksEmpty">No saved looks yet. Set up a look below, then save it.</p>
<div class="row" style="margin-top:12px"><button class="btn" id="saveLook">Save current look…</button></div>
<p class="err" id="lookErr" hidden></p>
</div>
<div class="card">
<label>Effect</label>
<select class="picker" id="modeSel"></select>
<p class="hint" id="modeHint"></p>
<div id="colorBox" style="margin-top:14px">
<label id="colorLabel">Color</label>
<div class="colorRow" id="colorRow"><input id="color" type="color"></div>
<div class="row" id="colorCountRow" style="margin-top:8px">
<button class="btn" id="colorLess">− color</button><button class="btn" id="colorMore">+ color</button><span class="hint" id="colorCountTxt" style="margin:0"></span>
</div>
</div>
<p class="hint" id="noColorHint" hidden>This effect makes its own colors.</p>
<label style="margin-top:14px">Brightness <span id="briVal"></span></label><input id="bri" type="range" min="0" max="255">
<details class="inner" id="setFold"><summary id="setSum"></summary>
<div id="spdBox"><label style="margin-top:10px"><span id="spdName"></span>: <span id="spdVal"></span></label><input id="spd" type="range" min="1" max="100"></div>
<div id="sizeBox"><label style="margin-top:14px"><span id="sizeName"></span>: <span id="sizeVal"></span></label><input id="size" type="range" min="1" max="30"></div>
<div id="fsecBox"><label style="margin-top:14px"><span id="fsecName"></span>: <span id="fsecVal"></span></label><input id="fsec" type="range" min="2" max="60"></div>
<div id="fminBox"><label style="margin-top:14px">Lowest level: <span id="fminVal"></span></label><input id="fmin" type="range" min="0" max="230">
<p class="hint">Below about 20% the dim end may show steps.</p></div>
<div id="dirBox"><label style="margin-top:14px">Direction</label>
<div class="seg" id="dir"><button data-rev="0"></button><button data-rev="1"></button></div></div>
<div id="bgBox"><label style="margin-top:14px">Background (lights between / behind the effect)</label>
<div class="seg" id="bg"><button data-bg="0">Off</button><button data-bg="1">On</button></div>
<input id="bgColor" type="color" style="margin-top:10px" hidden>
<p class="hint" id="bgHint" hidden>Uses the same brightness. Lights every light, so it adds current.</p></div>
</details>
</div>
<div class="applyBar" id="applyBar" hidden>
<span id="applyTxt">Not applied yet</span>
<button class="btn" id="discardBtn">Discard</button><button class="btn primary" id="applyBtn">Apply</button>
</div>
</section>

<!-- ===== House ===== -->
<section data-tab="house">
<p class="hint" style="margin:14px 0 8px">Add photos of the house, draw a line along the eave for each zone, and see the lights glow on the photo. Easiest on a PC.</p>
<iframe class="editor" id="editorFrame" title="House view editor"></iframe>
</section>

<!-- ===== Setup ===== -->
<section data-tab="setup">
<details class="fold" data-fold="strands"><summary>Strand lengths <small id="strandSum"></small></summary><div class="body">
<p class="hint" style="margin:0 0 8px">How many lights are on each strand (1–300). Zones, paths and the house view follow these numbers.</p>
<div class="row" style="align-items:end">
<div><label>Strand A (left)</label><input id="cntA" type="number" min="1" max="300"></div>
<div><label>Strand B (right)</label><input id="cntB" type="number" min="1" max="300"></div>
<button class="btn primary" id="cntSave">Save</button>
</div>
<p class="err" id="cntErr" hidden></p>
</div></details>

<details class="fold" data-fold="find"><summary>Find a light</summary><div class="body">
<p class="hint" style="margin:0 0 10px">Lights only the chosen light (green) so you can see where it is. Turns itself off after 5 minutes.</p>
<div class="seg" id="findStrand"><button data-s="A">Strand A (left)</button><button data-s="B">Strand B (right)</button></div>
<div class="row" style="margin-top:10px">
<button class="btn" id="findMinus">−</button><input id="findNum" type="number" min="1" value="1"><button class="btn" id="findPlus">+</button>
</div>
<div class="row" style="margin-top:10px"><button class="btn primary" id="findShow">Show light</button><button class="btn" id="findStop">Stop</button></div>
</div></details>

<details class="fold" data-fold="zones"><summary>Zones <small id="zoneSum"></small></summary><div class="body">
<p class="hint" style="margin:0 0 10px">A zone is a name plus lights, e.g. <b>A1-12</b> or <b>A1-12, B30-65</b>. <b>A12-1</b> runs backwards. <b>Click a zone</b> to light it white for 10 seconds. Zones that are on are drawn on top of everything else.</p>
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
</div></details>

<details class="fold" data-fold="paths"><summary>Paths <small id="pathSum"></small></summary><div class="body">
<p class="hint" style="margin:0 0 10px">A path runs an effect through zones in the order you pick. ⇄ runs a zone backwards. <b>Gap</b> adds invisible lights after a zone so the effect disappears for a moment, like a jump. Leave a zone out to skip it.</p>
<div id="pathList"></div>
<p class="hint" id="noPaths">No paths yet.</p>
<div class="sub">
<label id="pathFormTitle">New path</label>
<input id="pathName" type="text" placeholder="Name, e.g. Front sweep" maxlength="23">
<div class="stepHead" id="stepHead" hidden><span>Zone (in order)</span><span style="width:40px">Rev</span><span style="width:58px">Gap after</span><span style="width:90px"></span></div>
<div id="pathSteps"></div>
<p class="hint" id="pathNeedsZones" hidden>Make some zones first.</p>
<div class="row" style="margin-top:10px">
<button class="btn" id="pathAdd">+ Add zone</button><button class="btn primary" id="pathSave">Save path</button><button class="btn" id="pathCancel" hidden>Cancel</button>
</div>
<p class="err" id="pathErr" hidden></p>
</div>
</div></details>

<details class="fold" data-fold="backup"><summary>Backup &amp; restore</summary><div class="body">
<p class="hint" style="margin:0 0 10px">One file with everything: strand lengths, strand and Whole house settings, zones, paths, saved looks, scenes, house layout and photos. Keep it on your PC. Restoring replaces all of these on this board.</p>
<div class="row"><button class="btn primary" id="backupBtn">Download backup</button><button class="btn" id="restoreBtn">Restore from file…</button></div>
<input type="file" id="restoreFile" accept=".json,application/json" hidden>
<p class="hint" id="backupStatus"></p>
</div></details>

<details class="fold" data-fold="firmware"><summary>Firmware update <small id="fwSum"></small></summary><div class="body">
<p class="hint" style="margin:0 0 10px">Running v<span id="fwVer"></span>. Easiest: in the Arduino IDE pick <b>Tools → Port → eavelights</b> (network) and Upload. Or here: in the IDE use <b>Sketch → Export compiled Binary</b>, then choose the <b>.bin</b> from the sketch folder (not the merged/bootloader ones). If an update fails, the current firmware keeps running.</p>
<input type="file" id="fwFile" accept=".bin">
<input type="password" id="fwPass" placeholder="Update password (OTA_PASSWORD in secrets.h)" style="margin-top:8px">
<button class="btn primary" id="fwGo" style="margin-top:10px">Upload firmware</button>
<p class="hint" id="fwStatus"></p>
</div></details>
<p class="err" id="storageWarn" hidden>Storage not found, so zones, scenes and saved looks are lost on restart. Use Tools → Partition Scheme → 16M Flash (3MB APP/9.9MB FATFS).</p>
</section>

<p class="status" id="status">Connecting…</p>
</main><script>
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const pct=v=>Math.round(v/2.55)+'%';
const store={get(k,d){try{const v=localStorage.getItem(k);return v===null?d:JSON.parse(v);}catch(e){return d;}},
 set(k,v){try{localStorage.setItem(k,JSON.stringify(v));}catch(e){}}};
function showErr(id,msg){$(id).textContent=msg||'';$(id).hidden=!msg;}
let st=null,timer=null,looks=[],sceneList=[],layout=null,frame=null;

// ================= Tabs =================
let tab=store.get('eaveTab','home');
const editorFrame=$('editorFrame');
function setTab(t){tab=t;store.set('eaveTab',t);
 document.querySelectorAll('#tabs button').forEach(b=>b.classList.toggle('sel',b.dataset.t===t));
 document.querySelectorAll('section[data-tab]').forEach(s=>s.classList.toggle('show',s.dataset.tab===t));
 document.body.classList.toggle('wide',t==='house');
 // the live card lives on Home and Lights only
 if(t==='home')$('liveSlotHome').appendChild($('liveCard'));
 if(t==='lights')$('liveSlotLights').appendChild($('liveCard'));
 if(t==='home')loadLayout();
 if(t==='house'&&!editorFrame.src)editorFrame.src='/editor?embed=1';
 if(editorFrame.contentWindow)editorFrame.contentWindow.postMessage({eaveEditorActive:t==='house'},'*');
 if(t!=='house'&&layoutDirtyFromEditor){layoutDirtyFromEditor=false;loadLayout();}
 drawLive();}
document.querySelectorAll('#tabs button').forEach(b=>b.onclick=()=>setTab(b.dataset.t));
let layoutDirtyFromEditor=false;
window.addEventListener('message',e=>{if(e.data&&e.data.eaveEditorHeight){editorFrame.style.height=(e.data.eaveEditorHeight+20)+'px';layoutDirtyFromEditor=true;}});

// Remember which Setup sections are open
const openFolds=store.get('eaveFolds',[]);
document.querySelectorAll('details.fold').forEach(d=>{if(openFolds.includes(d.dataset.fold))d.open=true;
 d.addEventListener('toggle',()=>{const o=[...document.querySelectorAll('details.fold')].filter(x=>x.open).map(x=>x.dataset.fold);store.set('eaveFolds',o);});});

// ================= What is being controlled =================
// sel: 0/1 strand A/B, BOTH, HOUSE, ZONE+slot, PATH+slot
const BOTH=-1,HOUSE=-2,ZONE=100,PATH=200;
let sel=store.get('eaveSel',0);
const both=()=>sel===BOTH,house=()=>sel===HOUSE,pathSel=()=>sel>=PATH,zoneSel=()=>sel>=ZONE&&sel<PATH;
const zoneBySlot=n=>st.zones.find(z=>z.slot===n),pathBySlot=n=>st.paths.find(p=>p.slot===n);
function cur(){return house()?st.house:pathSel()?pathBySlot(sel-PATH):zoneSel()?zoneBySlot(sel-ZONE):st.outputs[both()?0:sel];}
function target(){return house()?'house':both()?'all':pathSel()?'p'+(sel-PATH):zoneSel()?'z'+(sel-ZONE):st.outputs[sel].id;}
function curName(){return house()?'Whole house':both()?'Both strands':(zoneSel()||pathSel())?cur().name:'Strand '+cur().id;}
function anyStrandOn(){return st.outputs.some(o=>o.on);}
// everything that is on, by name
function onNames(s){const n=[];s.outputs.forEach(o=>o.on&&n.push('Strand '+o.id));if(s.house.on)n.push('Whole house');
 s.paths.forEach(p=>p.on&&n.push(p.name));s.zones.forEach(z=>z.on&&n.push(z.name));return n;}

function renderPicker(s){const onOff=o=>o.on?'  ● on':'  ○ off';
 let h='<optgroup label="Strands">'+s.outputs.map((o,i)=>'<option value="'+i+'">Strand '+o.id+(o.id==='A'?' (left)':' (right)')+onOff(o)+'</option>').join('')+
  '<option value="'+BOTH+'">Both strands'+(s.outputs.every(o=>o.on)?'  ● on':anyStrandOn()?'  ◐ some on':'  ○ off')+'</option></optgroup>'+
  '<optgroup label="Whole house"><option value="'+HOUSE+'">Whole house'+onOff(s.house)+'</option></optgroup>';
 if(s.paths.length)h+='<optgroup label="Paths">'+s.paths.map(p=>'<option value="'+(PATH+p.slot)+'">'+esc(p.name)+onOff(p)+'</option>').join('')+'</optgroup>';
 if(s.zones.length)h+='<optgroup label="Zones">'+s.zones.map(z=>'<option value="'+(ZONE+z.slot)+'">'+esc(z.name)+onOff(z)+'</option>').join('')+'</optgroup>';
 $('picker').innerHTML=h;$('picker').value=String(sel);}
$('picker').onchange=async e=>{const next=+e.target.value;
 if(hasPending()){if(confirm('Apply your changes to '+pendingName+' first?\n\nOK = apply them, Cancel = throw them away.'))await applyPending();
  else{pending={};pendingTarget=null;updateApplyBar();}}
 sel=next;store.set('eaveSel',sel);show(st);};

function show(s){st=s;
 if((zoneSel()&&!zoneBySlot(sel-ZONE))||(pathSel()&&!pathBySlot(sel-PATH))||(sel>=0&&sel<ZONE&&sel>=s.outputs.length)||(sel<HOUSE))sel=0;
 const names=onNames(s);
 $('onPill').textContent=names.length?names.length+' on':'all off';$('onPill').classList.toggle('on',names.length>0);
 $('onList').textContent=names.length?names.join(' · '):'Everything is off.';
 $('allOff').textContent=names.length?'All lights off':s.canAllOn?'All lights on (as before)':'Everything is off';
 $('allOff').disabled=!names.length&&!s.canAllOn;$('allOff').classList.toggle('on',!names.length&&s.canAllOn);
 renderPicker(s);
 const h=s.house,zonesOn=s.zones.filter(z=>z.on),pathsOn=s.paths.filter(p=>p.on);
 const coverStrands=(h.on?['Whole house']:[]).concat(pathsOn.map(p=>p.name),zonesOn.map(z=>z.name));
 const o=cur(),lastA='A'+s.outputs[0].count,lastB='B'+(s.outputs[1]?s.outputs[1].count:'');
 let hint='';
 if(house())hint='One '+h.length+'-light line across the front: '+lastB+' (far right) → controller → '+lastA+' (far left). While on, it covers Strand A and B'+(zonesOn.length?'; zones that are on ('+zonesOn.map(z=>z.name).join(', ')+') are drawn on top':'')+'.';
 else if(zoneSel())hint=o.length+' lights: '+o.ranges+'. While on, drawn on top of everything else.';
 else if(pathSel())hint='Path: '+o.summary+' ('+o.length+' positions incl. gaps). Zones that are on are drawn on top of it.';
 else if(coverStrands.length)hint='Drawn on top of the strands right now: '+coverStrands.join(', ')+'.';
 else if(!both()&&!o.enabled)hint='This strand is turned off in the firmware (ENABLE_OUTPUT_B).';
 $('tabHint').textContent=hint;$('tabHint').hidden=!hint;
 const isOn=both()?anyStrandOn():o.on;
 $('power').textContent=curName()+' — '+(isOn?'ON':'OFF');$('power').classList.toggle('on',isOn);
 $('lookTarget').textContent='· tap to apply to '+curName();
 if(hasPending()&&pendingTarget===target())applyPendingTo(o,pending);   // keep showing unapplied changes
 renderLook(o);updateApplyBar();
 // setup
 $('strandSum').textContent='A '+s.outputs[0].count+(s.outputs[1]?' · B '+s.outputs[1].count:'');
 $('zoneSum').textContent=s.zones.length+' of 12';$('pathSum').textContent=s.paths.length+' of 8';$('fwSum').textContent='v'+s.version;
 $('zoneList').innerHTML=s.zones.map(z=>'<div class="zoneRow click'+(z.slot===showingZone?' showing':'')+'" data-show="'+z.slot+'" title="Click to light this zone white for 10 seconds"><div>'+esc(z.name)+
  '<small>'+esc(z.ranges)+' · '+z.length+' lights'+(z.slot===showingZone?' · showing on the house…':'')+'</small></div>'+
  '<button class="btn" data-edit="'+z.slot+'">Edit</button><button class="btn danger" data-del="'+z.slot+'">Delete</button></div>').join('');
 $('noZones').hidden=s.zones.length>0;
 $('zoneList').querySelectorAll('[data-edit]').forEach(b=>b.onclick=e=>{e.stopPropagation();startEdit(+b.dataset.edit);});
 $('zoneList').querySelectorAll('[data-del]').forEach(b=>b.onclick=e=>{e.stopPropagation();delZone(+b.dataset.del);});
 $('zoneList').querySelectorAll('[data-show]').forEach(r=>r.onclick=()=>showZone(+r.dataset.show));
 $('pathList').innerHTML=s.paths.map(p=>'<div class="zoneRow"><div>'+esc(p.name)+'<small>'+esc(p.summary)+' · '+p.length+' positions</small></div>'+
  '<button class="btn" data-pedit="'+p.slot+'">Edit</button><button class="btn danger" data-pdel="'+p.slot+'">Delete</button></div>').join('');
 $('noPaths').hidden=s.paths.length>0;
 $('pathList').querySelectorAll('[data-pedit]').forEach(b=>b.onclick=()=>startPathEdit(+b.dataset.pedit));
 $('pathList').querySelectorAll('[data-pdel]').forEach(b=>b.onclick=()=>delPath(+b.dataset.pdel));
 renderSteps();
 if(document.activeElement!==$('cntA'))$('cntA').value=s.outputs[0].count;
 if(s.outputs[1]&&document.activeElement!==$('cntB'))$('cntB').value=s.outputs[1].count;
 $('fwVer').textContent=s.version;$('findNum').max=findMax();
 $('findStrand').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',b.dataset.s===findStrand));
 $('storageWarn').hidden=s.storage;
 $('locateBanner').hidden=!s.locate.active;
 $('locateBanner').textContent=s.locate.active?'Find a light is on: showing only '+s.locate.ranges+'. Stop it in Setup → Find a light.':'';
 $('status').textContent='Connected · v'+s.version;}

async function post(url,p){const r=await fetch(url,{method:'POST',body:new URLSearchParams(p)});return r.json();}

// ---- Apply mode: look changes wait on the page until Apply (on/off stays instant) ----
let applyMode=store.get('eaveApplyMode','apply'),pending={},pendingTarget=null,pendingName='';
const LOOK_KEYS=['color','brightness','mode','fadeSeconds','fadeMin','speed','size','reverse','bgOn','bgColor','colorCount','color2','color3','color4','color5'];
const hasPending=()=>Object.keys(pending).length>0;
// Shows pending values on the page's copy of the settings (the lights don't change yet).
function applyPendingTo(o,p){if(!o)return;
 for(const [k,v] of Object.entries(p)){
  if(k==='color'||k==='bgColor')o[k]='#'+String(v).toUpperCase();
  else if(k==='mode')o.mode=v;
  else if(k==='reverse'||k==='bgOn')o[k]=(v==1||v===true||v==='true');
  else if(/^color[2-5]$/.test(k)){o.colors=o.colors||[];o.colors[+k.slice(5)-2]='#'+String(v).toUpperCase();}
  else o[k]=+v;}}
function updateApplyBar(){$('applyBar').hidden=!hasPending();$('applyTxt').textContent='Not applied yet: '+pendingName;
 $('applyMode').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',b.dataset.am===applyMode));}
async function applyPending(){if(!hasPending())return;const p=Object.assign({output:pendingTarget},pending);
 pending={};pendingTarget=null;updateApplyBar();
 try{const j=await post('/api/state',p);if(j.error)$('status').textContent=j.error;else show(j);}catch(e){$('status').textContent='Not connected';}}
async function discardPending(){pending={};pendingTarget=null;updateApplyBar();
 try{show(await (await fetch('/api/state')).json());}catch(e){}}
$('applyBtn').onclick=applyPending;$('discardBtn').onclick=discardPending;
$('applyMode').querySelectorAll('button').forEach(b=>b.onclick=async()=>{applyMode=b.dataset.am;store.set('eaveApplyMode',applyMode);
 if(applyMode==='instant')await applyPending();updateApplyBar();});
window.addEventListener('beforeunload',e=>{if(hasPending()){e.preventDefault();e.returnValue='';}});

async function send(p){if(!st)return;
 if(applyMode==='apply'&&Object.keys(p).every(k=>LOOK_KEYS.includes(k))){
  if(pendingTarget!==target()){pending={};pendingTarget=target();pendingName=curName();}
  Object.assign(pending,p);applyPendingTo(cur(),p);renderLook(cur());updateApplyBar();return;}
 p.output=target();
 try{const j=await post('/api/state',p);if(j.error){$('status').textContent=j.error;}else show(j);}
 catch(e){$('status').textContent='Not connected';}}
function sendSoon(p){clearTimeout(timer);timer=setTimeout(()=>send(p),120);}
// Both: if any strand is on, turn both off; otherwise turn both on.
$('power').onclick=()=>send({on:(both()?anyStrandOn():cur().on)?0:1});
// All lights off, or (when everything is off) back on as it was before.
$('allOff').onclick=async()=>{const on=onNames(st).length>0;
 const j=await post(on?'/api/all-off':'/api/all-on',{});if(j.error)$('status').textContent=j.error;else show(j);};
$('color').oninput=e=>sendSoon({color:e.target.value.slice(1)});
$('bri').oninput=e=>{$('briVal').textContent=pct(e.target.value);sendSoon({brightness:e.target.value});};
$('modeSel').onchange=e=>send({mode:e.target.value});
$('fsec').oninput=e=>{$('fsecVal').textContent=e.target.value+' s';sendSoon({fadeSeconds:e.target.value});};
$('fmin').oninput=e=>{$('fminVal').textContent=pct(e.target.value);sendSoon({fadeMin:e.target.value});};
$('spd').oninput=e=>{const m=modeById(cur().mode),v=+e.target.value;$('spdVal').textContent=m.fmt?m.fmt(v):v;sendSoon({speed:v});};
$('size').oninput=e=>{const m=modeById(cur().mode),v=+e.target.value;$('sizeVal').textContent=m.sizeFmt?m.sizeFmt(v):v+(v===1?' light':' lights');sendSoon({size:v});};
$('dir').querySelectorAll('button').forEach(b=>b.onclick=()=>send({reverse:b.dataset.rev}));
$('bg').querySelectorAll('button').forEach(b=>b.onclick=()=>send({bgOn:b.dataset.bg}));
$('bgColor').oninput=e=>sendSoon({bgColor:e.target.value.slice(1)});
// remember whether the mode fold-outs are open
// remember whether the effect settings fold-out is open
$('setFold').open=store.get('eaveSetFold',false);$('setFold').addEventListener('toggle',()=>store.set('eaveSetFold',$('setFold').open));

// ================= Effects: what each one uses =================
// colors: 0 = makes its own, 1 = one color, 'multi' = color list (min colors).
// spd/size/fsec: label for that slider (missing = not used); fmt formats the speed value.
// dir: true = show direction (or [forward label, reverse label] to override); bg: background.
const lps=v=>v+' lights/s',still=v=>v<=1?'still':v+' lights/s';
const MODES=[
 {g:'Basic',id:'solid',name:'Solid',colors:1,hint:'One steady color.'},
 {g:'Basic',id:'fade',name:'Fade (breathe)',colors:1,fsec:'Seconds per breath',fmin:1,hint:'The color slowly breathes in and out.'},
 {g:'Basic',id:'test',name:'Test colors',colors:0,hint:'Red, green, blue, white, off: for checking lights and current.'},
 {g:'Motion',id:'chase',name:'Chase',colors:1,spd:'Speed',fmt:lps,size:'Lit block length',dir:true,bg:1,hint:'Blocks of light with gaps, moving along.'},
 {g:'Motion',id:'comet',name:'Comet',colors:1,spd:'Speed',fmt:lps,size:'Tail length',dir:true,bg:1,hint:'A bright head with a fading tail.'},
 {g:'Motion',id:'bounce',name:'Bounce (scanner)',colors:1,spd:'Speed',fmt:lps,size:'Tail length',bg:1,hint:'A comet sweeping back and forth.'},
 {g:'Motion',id:'center',name:'Center out / in',colors:'multi',spd:'Speed',fmt:lps,size:'Ripple width',dir:['Outward','Inward'],bg:1,hint:'Ripples from the middle (the controller, on Whole house) to the ends, or back in. Colors alternate per ripple.'},
 {g:'Motion',id:'meteor',name:'Meteor',colors:1,spd:'Speed',fmt:lps,size:'Tail length',dir:true,bg:1,hint:'A comet with a broken, sparkly trail.'},
 {g:'Motion',id:'running',name:'Running lights',colors:1,spd:'Speed',fmt:lps,size:'Wave length',dir:true,bg:1,hint:'A smooth wave of brightness rolling along.'},
 {g:'Motion',id:'wipe',name:'Color wipe',colors:'multi',min:1,spd:'Speed',fmt:lps,dir:true,hint:'Fills light by light with each color in turn. One color: fills, then wipes off.'},
 {g:'Color',id:'rainbow',name:'Rainbow wave',colors:0,spd:'Speed',fmt:lps,size:'Rainbow length (×5 lights)',dir:true,hint:'A rainbow scrolling along the lights.'},
 {g:'Color',id:'rainbowcycle',name:'Rainbow cycle',colors:0,spd:'Speed',fmt:v=>Math.round(600/v)+' s per rainbow',hint:'All lights one color, slowly going through the rainbow.'},
 {g:'Color',id:'gradient',name:'Gradient',colors:'multi',min:2,spd:'Drift',fmt:still,dir:true,hint:'Blends through your colors from one end to the other. Drift 1 = still.'},
 {g:'Holiday',id:'stripes',name:'Stripes / candy cane',colors:'multi',min:2,spd:'Slide',fmt:still,size:'Stripe width',dir:true,hint:'Bands of your colors (red/white, red/green…). Slide 1 = still.'},
 {g:'Holiday',id:'alternate',name:'Alternating swap',colors:'multi',min:2,spd:'Swap speed',fmt:v=>(10/v).toFixed(v>=10?1:0)+' s per swap',size:'Group size',hint:'Every other light (or group) in your colors, swapping on a beat.'},
 {g:'Holiday',id:'colorcycle',name:'Color cycle',colors:'multi',min:2,fsec:'Seconds per color',hint:'All lights hold each color, then fade to the next.'},
 {g:'Holiday',id:'twinkle',name:'Twinkle',colors:'multi',min:1,spd:'Twinkle speed',fmt:v=>(v/10).toFixed(1)+'×',size:'Density',sizeFmt:v=>Math.min(v*3,90)+'%',bg:1,hint:'Lights gently twinkle in your colors over the background.'},
 {g:'Sparkle',id:'sparkle',name:'Sparkle',colors:'multi',min:1,colorNames:['Base','Sparkle'],spd:'Rate',fmt:v=>v,size:'Density',hint:'Quick flashes of color 2 (or white) over color 1.'},
 {g:'Sparkle',id:'fire',name:'Fire / candle',colors:1,spd:'Flicker speed',fmt:v=>v,hint:'Warm candle-like flicker. Orange or amber works best.'},
 {g:'Sparkle',id:'lightning',name:'Lightning',colors:1,spd:'How often',fmt:v=>'about every '+Math.round(60/v)+' s',bg:1,hint:'Occasional flashes and flickers; dark (or background) in between.'},
 {g:'Sparkle',id:'strobe',name:'Strobe / flash',colors:1,spd:'Flash rate',fmt:v=>(v/10).toFixed(1)+' per second',bg:1,hint:'Fast on/off flashing. Use sparingly: it can bother neighbors and drivers.'},
];
const modeById=id=>MODES.find(m=>m.id===id)||MODES[0];
(function buildModeSelect(){const groups=[...new Set(MODES.map(m=>m.g))];
 $('modeSel').innerHTML=groups.map(g=>'<optgroup label="'+g+'">'+MODES.filter(m=>m.g===g).map(m=>'<option value="'+m.id+'">'+m.name+'</option>').join('')+'</optgroup>').join('');})();

// The current colors (1-5) of a channel: color, then colors 2-5.
const colorsOf=o=>[o.color].concat(o.colors||[]).slice(0,Math.max(1,o.colorCount||1));
function renderLook(o){const m=modeById(o.mode);
 $('modeSel').value=m.id;$('modeHint').textContent=m.hint||'';
 // colors
 $('colorBox').hidden=!m.colors;$('noColorHint').hidden=!!m.colors;
 const multi=m.colors==='multi',list=multi?colorsOf(o):[o.color];
 $('colorLabel').textContent=multi?(m.colorNames?m.colorNames.join(' / ')+' colors':'Colors (in order)'):'Color';
 const row=$('colorRow');
 while(row.children.length>list.length)row.removeChild(row.lastChild);
 while(row.children.length<list.length){const k=row.children.length,inp=document.createElement('input');inp.type='color';
  inp.oninput=e=>sendSoon({['color'+(k+1)]:e.target.value.slice(1)});row.appendChild(inp);}
 list.forEach((c,k)=>{if(document.activeElement!==row.children[k])row.children[k].value=c.toLowerCase();});
 $('colorCountRow').hidden=!multi;
 $('colorLess').disabled=list.length<=(m.min||1);$('colorMore').disabled=list.length>=5;
 $('colorCountTxt').textContent=list.length+' of 5';
 $('bri').value=o.brightness;$('briVal').textContent=pct(o.brightness);
 // settings: show only what this effect uses
 const [fwd,rev]=Array.isArray(m.dir)?m.dir:house()?['Right → left','Left → right']:zoneSel()?['As listed','Reversed']:pathSel()?['In path order','Reversed']
  :o.id==='A'?['Light 1 → '+o.count+' (outward, left)','Light '+o.count+' → 1 (inward)']
  :o.id==='B'?['Light 1 → '+o.count+' (outward, right)','Light '+o.count+' → 1 (inward)']:['Outward','Inward'];
 $('dir').children[0].textContent=fwd;$('dir').children[1].textContent=rev;
 const parts=[];
 $('spdBox').hidden=!m.spd;if(m.spd){$('spdName').textContent=m.spd;$('spd').value=o.speed;$('spdVal').textContent=m.fmt(o.speed);parts.push(m.spd.toLowerCase()+' '+m.fmt(o.speed));}
 $('sizeBox').hidden=!m.size;if(m.size){$('sizeName').textContent=m.size;$('size').value=o.size;const sv=m.sizeFmt?m.sizeFmt(o.size):o.size+(o.size===1?' light':' lights');$('sizeVal').textContent=sv;parts.push(m.size.toLowerCase().split(' (')[0]+' '+sv);}
 $('fsecBox').hidden=!m.fsec;if(m.fsec){$('fsecName').textContent=m.fsec;$('fsec').value=o.fadeSeconds;$('fsecVal').textContent=o.fadeSeconds+' s';parts.push(o.fadeSeconds+' s');}
 $('fminBox').hidden=!m.fmin;if(m.fmin){$('fmin').value=o.fadeMin;$('fminVal').textContent=pct(o.fadeMin);parts.push('lowest '+pct(o.fadeMin));}
 $('dirBox').hidden=!m.dir;if(m.dir)parts.push((o.reverse?rev:fwd).split(' (')[0]);
 $('bgBox').hidden=!m.bg;if(m.bg)parts.push('background '+(o.bgOn?'on':'off'));
 $('setFold').hidden=!parts.length;$('setSum').textContent='Settings · '+parts.join(' · ');
 $('dir').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',(b.dataset.rev==='1')===o.reverse));
 $('bg').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',(b.dataset.bg==='1')===o.bgOn));
 $('bgColor').hidden=!o.bgOn;$('bgHint').hidden=!o.bgOn;$('bgColor').value=o.bgColor.toLowerCase();}
$('colorLess').onclick=()=>{const o=cur();send({colorCount:Math.max(1,colorsOf(o).length-1)});};
$('colorMore').onclick=()=>{const o=cur();send({colorCount:Math.min(5,colorsOf(o).length+1)});};

// ================= Saved looks =================
function showLooks(list){looks=list||[];
 $('looks').innerHTML=looks.map(p=>{const m=modeById(p.mode),cs=m.colors==='multi'?colorsOf(p):m.colors?[p.color]:[];
  return '<span class="chip" data-id="'+p.id+'" title="'+esc(m.name)+'">'+
  (m.colors?'':'<span class="dot" style="background:linear-gradient(90deg,red,yellow,lime,cyan,blue,magenta)"></span>')+
  cs.map(c=>'<span class="dot" style="background:'+c+'"></span>').join('')+
  (p.bgOn&&m.bg?'<span class="dot" style="background:'+p.bgColor+'"></span>':'')+
  esc(p.name)+'<span class="x" data-x="'+p.id+'" title="Delete">×</span></span>';}).join('');
 $('looksEmpty').hidden=looks.length>0;
 $('looks').querySelectorAll('.chip').forEach(c=>c.onclick=async e=>{
  const x=e.target.dataset.x;
  if(x!==undefined){const p=looks.find(l=>l.id===+x);if(!confirm('Delete saved look "'+p.name+'"?'))return;
   const j=await post('/api/presets',{action:'delete',id:x});if(j.error)showErr('lookErr',j.error);else{showErr('lookErr');showLooks(j.presets);}return;}
  if(pendingTarget===target()){pending={};pendingTarget=null;}   // the look replaces unapplied changes
  const j=await post('/api/presets',{action:'apply',id:c.dataset.id,output:target()});
  if(j.error)showErr('lookErr',j.error);else{showErr('lookErr');show(j);}});}
$('saveLook').onclick=async()=>{if(hasPending()&&!confirm('Saving uses the settings the lights have now. Your unapplied changes will be applied first. Continue?'))return;
 await applyPending();const name=prompt('Name for this look (e.g. Xmas, Valentines). Saves the current settings of '+curName()+'. Same name replaces it.');
 if(!name)return;const j=await post('/api/presets',{action:'save',name,output:target()});
 if(j.error)showErr('lookErr',j.error);else{showErr('lookErr');showLooks(j.presets);}};

// ================= Scenes =================
function showScenes(list){sceneList=list||[];
 $('scenes').innerHTML=sceneList.map(s=>'<span class="chip" data-id="'+s.id+'" title="'+s.on+' parts on">'+
  s.colors.map(c=>'<span class="dot" style="background:'+c+'"></span>').join('')+esc(s.name)+
  '<span class="x" data-x="'+s.id+'" title="Delete">×</span></span>').join('');
 $('scenesEmpty').hidden=sceneList.length>0;
 $('scenes').querySelectorAll('.chip').forEach(c=>c.onclick=async e=>{
  const x=e.target.dataset.x;
  if(x!==undefined){const s=sceneList.find(l=>l.id===+x);if(!confirm('Delete scene "'+s.name+'"?'))return;
   const j=await post('/api/scenes',{action:'delete',id:x});if(j.error)showErr('sceneErr',j.error);else{showErr('sceneErr');showScenes(j.scenes);}return;}
  pending={};pendingTarget=null;   // a scene replaces any unapplied changes
  const j=await post('/api/scenes',{action:'apply',id:c.dataset.id});
  if(j.error)showErr('sceneErr',j.error);else{showErr('sceneErr');show(j);}});}
$('saveScene').onclick=async()=>{if(hasPending()&&!confirm('Your unapplied changes to '+pendingName+' will be applied first so they are part of the scene. Continue?'))return;
 await applyPending();const name=prompt('Name for this scene (e.g. Xmas, Halloween, Normal). Saves what every strand, zone and path is doing right now. Same name replaces it.');
 if(!name)return;const j=await post('/api/scenes',{action:'save',name});
 if(j.error)showErr('sceneErr',j.error);else{showErr('sceneErr');showScenes(j.scenes);}};
function loadLooksAndScenes(){return fetch('/api/presets').then(r=>r.json()).then(j=>{showLooks(j.presets);showScenes(j.scenes);}).catch(()=>{});}

// ================= Setup: strand lengths =================
$('cntSave').onclick=async()=>{const j=await post('/api/strands',{a:$('cntA').value,b:$('cntB').value});
 if(j.error)showErr('cntErr',j.error);else{showErr('cntErr');show(j);}};

// ================= Setup: find a light =================
let findStrand='A';
function findMax(){const o=st.outputs.find(x=>x.id===findStrand);return o?o.count:1;}
function findNum(){return Math.min(findMax(),Math.max(1,parseInt($('findNum').value)||1));}
async function locate(ranges){const j=await post('/api/locate',ranges?{ranges}:{stop:1});if(j.error)$('status').textContent=j.error;else show(j);return j;}
function findShow(){$('findNum').value=findNum();locate(findStrand+findNum());}
$('findStrand').querySelectorAll('button').forEach(b=>b.onclick=()=>{findStrand=b.dataset.s;show(st);if(st.locate.active)findShow();});
$('findMinus').onclick=()=>{$('findNum').value=Math.max(1,findNum()-1);findShow();};
$('findPlus').onclick=()=>{$('findNum').value=Math.min(findMax(),findNum()+1);findShow();};
$('findNum').onchange=findShow;$('findShow').onclick=findShow;$('findStop').onclick=()=>locate('');

// ================= Setup: zones =================
// Click a zone in the list: its lights go white for 10 s, then back to normal.
const SHOW_SECONDS=10;let showingZone=-1,showTimer=null,editZone=-1;
async function showZone(slot){clearTimeout(showTimer);
 if(showingZone===slot){showingZone=-1;await locate('');return;}
 const z=zoneBySlot(slot);if(!z)return;showingZone=slot;
 const j=await post('/api/locate',{ranges:z.ranges,seconds:SHOW_SECONDS,white:1});
 if(j.error){showingZone=-1;showErr('zoneErr',j.error);}else{showErr('zoneErr');show(j);}
 showTimer=setTimeout(async()=>{showingZone=-1;try{show(await (await fetch('/api/state')).json());}catch(e){}},SHOW_SECONDS*1000+300);}
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

// ================= Setup: paths =================
let editPath=-1,steps=[];  // steps: {zone, reverse, gap} while building a path
function renderSteps(){if(!st)return;
 const zs=st.zones;$('pathNeedsZones').hidden=zs.length>0;$('stepHead').hidden=!steps.length;
 $('pathSteps').innerHTML=steps.map((s,i)=>'<div class="stepRow"><select data-z="'+i+'">'+
  zs.map(z=>'<option value="'+z.slot+'"'+(z.slot===s.zone?' selected':'')+'>'+esc(z.name)+' ('+esc(z.ranges)+')</option>').join('')+
  (zs.some(z=>z.slot===s.zone)?'':'<option selected value="-1">(deleted zone)</option>')+'</select>'+
  '<button class="btn'+(s.reverse?' primary':'')+'" data-r="'+i+'" title="Run this zone backwards">⇄</button>'+
  '<input type="number" min="0" max="30" value="'+s.gap+'" data-g="'+i+'" title="Invisible lights after this zone">'+
  '<button class="btn" data-u="'+i+'" title="Move up"'+(i?'':' disabled')+'>↑</button>'+
  '<button class="btn danger" data-x="'+i+'" title="Remove">×</button></div>').join('');
 const q=a=>$('pathSteps').querySelectorAll('['+a+']');
 q('data-z').forEach(e=>e.onchange=()=>{steps[+e.dataset.z].zone=+e.value;});
 q('data-r').forEach(e=>e.onclick=()=>{const s=steps[+e.dataset.r];s.reverse=!s.reverse;renderSteps();});
 q('data-g').forEach(e=>e.onchange=()=>{steps[+e.dataset.g].gap=Math.min(30,Math.max(0,parseInt(e.value)||0));});
 q('data-u').forEach(e=>e.onclick=()=>{const i=+e.dataset.u;[steps[i-1],steps[i]]=[steps[i],steps[i-1]];renderSteps();});
 q('data-x').forEach(e=>e.onclick=()=>{steps.splice(+e.dataset.x,1);renderSteps();});}
function resetPathForm(){editPath=-1;steps=[];$('pathName').value='';$('pathFormTitle').textContent='New path';$('pathCancel').hidden=true;showErr('pathErr');renderSteps();}
function startPathEdit(slot){const p=pathBySlot(slot);editPath=slot;$('pathName').value=p.name;
 steps=p.steps.map(s=>({zone:s.zone,reverse:s.reverse,gap:s.gap}));
 $('pathFormTitle').textContent='Edit path: '+p.name;$('pathCancel').hidden=false;showErr('pathErr');renderSteps();$('pathName').focus();}
async function delPath(slot){const p=pathBySlot(slot);if(!confirm('Delete path "'+p.name+'"?'))return;
 const j=await post('/api/paths',{action:'delete',id:slot});if(j.error)showErr('pathErr',j.error);else{if(editPath===slot)resetPathForm();show(j);}}
$('pathAdd').onclick=()=>{const zs=st.zones;if(!zs.length){showErr('pathErr','Make some zones first.');return;}
 const used=steps.map(s=>s.zone),next=zs.find(z=>!used.includes(z.slot))||zs[0];
 steps.push({zone:next.slot,reverse:false,gap:0});renderSteps();};
$('pathSave').onclick=async()=>{const p={action:'save',name:$('pathName').value,steps:steps.map(s=>s.zone+':'+(s.reverse?1:0)+':'+s.gap).join(',')};
 if(editPath>=0)p.id=editPath;const j=await post('/api/paths',p);if(j.error){showErr('pathErr',j.error);return;}
 resetPathForm();show(j);};
$('pathCancel').onclick=resetPathForm;

// ================= Setup: backup & restore =================
function toB64(u8){let s='';for(let i=0;i<u8.length;i+=0x8000)s+=String.fromCharCode.apply(null,u8.subarray(i,i+0x8000));return btoa(s);}
function fromB64(b){const s=atob(b),u=new Uint8Array(s.length);for(let i=0;i<s.length;i++)u[i]=s.charCodeAt(i);return u;}
// Settings of a strand / Whole house in the form /api/state accepts.
function chanFields(o){const f={on:o.on?1:0,color:o.color,brightness:o.brightness,mode:o.mode,fadeSeconds:o.fadeSeconds,fadeMin:o.fadeMin,
 speed:o.speed,size:o.size,reverse:o.reverse?1:0,bgOn:o.bgOn?1:0,bgColor:o.bgColor};
 if(o.colors){f.colorCount=o.colorCount;o.colors.forEach((c,k)=>f['color'+(k+2)]=c);}
 return f;}
$('backupBtn').onclick=async()=>{const say=t=>$('backupStatus').textContent=t;
 try{say('Collecting files…');const s=await (await fetch('/api/state')).json();
  const list=(await (await fetch('/api/files')).json()).files||[],files=[];
  for(const f of list){say('Reading '+f.path+'…');const r=await fetch('/fs'+f.path,{cache:'no-store'});if(!r.ok)throw new Error('could not read '+f.path);
   files.push({path:f.path,data:toB64(new Uint8Array(await r.arrayBuffer()))});}
  const backup={format:'eave-lights-backup',version:1,created:new Date().toISOString(),firmware:s.version,
   strands:s.outputs.map(o=>({id:o.id,count:o.count})),
   channels:Object.assign({house:chanFields(s.house)},...s.outputs.map(o=>({[o.id.toLowerCase()]:chanFields(o)}))),files};
  const blob=new Blob([JSON.stringify(backup)],{type:'application/json'}),a=document.createElement('a');
  a.href=URL.createObjectURL(blob);a.download='eave-lights-backup-'+new Date().toISOString().slice(0,10)+'.json';a.click();
  setTimeout(()=>URL.revokeObjectURL(a.href),5000);say('Backup downloaded ('+files.length+' files, '+Math.round(blob.size/1024)+' KB).');}
 catch(e){say('Backup failed: '+e.message);}};
$('restoreBtn').onclick=()=>$('restoreFile').click();
$('restoreFile').onchange=async e=>{const f=e.target.files[0];e.target.value='';if(!f)return;const say=t=>$('backupStatus').textContent=t;
 let b;try{b=JSON.parse(await f.text());}catch(err){say('That file is not a backup.');return;}
 if(b.format!=='eave-lights-backup'){say('That file is not an Eave Lights backup.');return;}
 if(!confirm('Restore the backup from '+(b.created||'?').slice(0,10)+'? This replaces zones, paths, looks, scenes, layout and photos on the board.'))return;
 let failed='';
 try{say('Starting restore…');let j=await post('/api/restore-begin',{});if(j.error)throw new Error(j.error);
  for(const file of b.files||[]){say('Restoring '+file.path+'…');const fd=new FormData();fd.append('file',new Blob([fromB64(file.data)]),'data');
   const r=await fetch('/api/restore-file?path='+encodeURIComponent(file.path),{method:'POST',body:fd});j=await r.json();
   if(j.error)failed+=file.path+' ('+j.error+') ';}}
 catch(err){failed+=err.message;}
 try{say('Reloading…');await post('/api/restore-finish',{});
  const cnt={};(b.strands||[]).forEach(s=>cnt[s.id.toLowerCase()]=s.count);
  if(cnt.a||cnt.b)await post('/api/strands',cnt);
  for(const [id,fields] of Object.entries(b.channels||{}))await post('/api/state',Object.assign({output:id},fields));
  await loadLooksAndScenes();show(await (await fetch('/api/state')).json());layout=null;loadLayout();
  if(editorFrame.src)editorFrame.src='/editor?embed=1';
  say(failed?'Restored, but these had problems: '+failed:'Restore complete.');}
 catch(err){say('Restore failed: '+err.message+'. Reload the page and try again.');}};

// ================= Setup: firmware update =================
$('fwGo').onclick=()=>{const f=$('fwFile').files[0];if(!f){$('fwStatus').textContent='Choose the .bin file first.';return;}
 if(!confirm('Update firmware with '+f.name+'? The lights go dark and the board restarts.'))return;
 const fd=new FormData();fd.append('file',f,f.name);const x=new XMLHttpRequest();
 x.open('POST','/api/firmware?password='+encodeURIComponent($('fwPass').value));
 x.upload.onprogress=e=>{if(e.lengthComputable)$('fwStatus').textContent='Uploading '+Math.round(e.loaded*100/e.total)+'%…';};
 x.onload=()=>{let j={};try{j=JSON.parse(x.responseText);}catch(e){}
  if(j.ok){$('fwStatus').textContent='Done. The board is restarting; this page reloads in 15 seconds.';setTimeout(()=>location.reload(),15000);}
  else $('fwStatus').textContent='Failed: '+(j.error||('HTTP '+x.status))+'. The current firmware is still running.';};
 x.onerror=()=>{$('fwStatus').textContent='Upload failed (connection lost). The current firmware is still running.';};
 $('fwStatus').textContent='Uploading…';x.send(fd);};

// ================= Live view =================
// Frame from /api/frame: [outputs], per output [count lo][count hi], then RGB per light, A first.
// LED values are linear; the screen is gamma-encoded, so dim values are lifted to look similar.
const GAM=new Uint8Array(256).map((_,v)=>Math.round(255*Math.pow(v/255,1/2.2)));
function frameInfo(b){const outs=b[0],cnt=[],start=[];let off=1+outs*2;
 for(let o=0;o<outs;o++){cnt.push(b[1+o*2]|(b[2+o*2]<<8));start.push(off);off+=cnt[o]*3;}return {outs,cnt,start};}
function lightRGB(fi,o,i){if(o>=fi.outs||i>=fi.cnt[o])return null;const k=fi.start[o]+i*3,r=frame[k],g=frame[k+1],b=frame[k+2];
 return (r|g|b)?[GAM[r],GAM[g],GAM[b]]:null;}
let liveMode=store.get('eaveLiveMode','photo'),photoView=store.get('eavePhotoView',0),frames=0,fpsT=performance.now();
$('liveMode').querySelectorAll('button').forEach(b=>b.onclick=()=>{liveMode=b.dataset.lm;store.set('eaveLiveMode',liveMode);drawLive();});

// --- strip: as seen from the street, A runs left from the controller, B right ---
const GAP=3;let stripCv=null,stripKey='';
function drawStrip(fi){const cA=fi.cnt[0],cB=fi.outs>1?fi.cnt[1]:0,key=cA+'/'+cB;
 if(!stripCv||stripKey!==key){stripKey=key;$('stripBox').innerHTML='<div class="liveRow"><canvas></canvas></div>'+
  '<div class="liveLabels"><span>A'+cA+' · left</span><span>controller</span><span>right · B'+cB+'</span></div>';
  stripCv=$('stripBox').querySelector('canvas');}
 const dpr=window.devicePixelRatio||1,w=stripCv.clientWidth,h=stripCv.clientHeight;if(!w)return;
 if(stripCv.width!==Math.round(w*dpr)){stripCv.width=Math.round(w*dpr);stripCv.height=Math.round(h*dpr);}
 const g=stripCv.getContext('2d');g.setTransform(dpr,0,0,dpr,0,0);g.clearRect(0,0,w,h);
 const step=w/(cA+GAP+cB),r=Math.max(1,Math.min(step*0.4,6));
 g.shadowBlur=0;g.fillStyle='#55554f';g.fillRect(step*(cA+GAP/2)-1,h*0.2,2,h*0.6);
 for(let o=0;o<Math.min(fi.outs,2);o++)for(let i=0;i<fi.cnt[o];i++){const c=lightRGB(fi,o,i);
  g.fillStyle=c?'rgb('+c.join(',')+')':'#2a2a27';g.shadowBlur=c?r*3:0;g.shadowColor=g.fillStyle;
  const slot=o===0?cA-1-i:cA+GAP+i;g.beginPath();g.arc(step*(slot+0.5),h/2,r,0,Math.PI*2);g.fill();}}

// --- house photo: same placement maths as the House editor ---
const imgs={};
function loadLayout(){fetch('/files/layout.json',{cache:'no-store'}).then(r=>r.ok?r.json():null).then(j=>{layout=j&&j.views?j:null;renderPhotoViews();drawLive();}).catch(()=>{layout=null;renderPhotoViews();drawLive();});}
function renderPhotoViews(){const vs=layout?layout.views:[];if(photoView>=vs.length)photoView=0;
 $('photoViews').innerHTML=vs.length>1?vs.map((v,i)=>'<button data-v="'+i+'" class="'+(i===photoView?'sel':'')+'">'+esc(v.name)+'</button>').join(''):'';
 $('photoViews').querySelectorAll('button').forEach(b=>b.onclick=()=>{photoView=+b.dataset.v;store.set('eavePhotoView',photoView);renderPhotoViews();drawLive();});}
function parseRanges(t){const out=[];
 for(const part of String(t).split(',')){const m=part.trim().match(/^([AB])(\d+)(?:\s*-\s*[AB]?(\d+))?$/i);if(!m)continue;
  const o=m[1].toUpperCase()==='A'?0:1,a=+m[2]-1,b=(m[3]?+m[3]:+m[2])-1,s=a<=b?1:-1;
  for(let i=a;;i+=s){out.push([o,i]);if(i===b)break;}}
 return out;}
function itemLights(id){if(!st)return [];
 if(id[0]==='z'){const z=zoneBySlot(+id.slice(1));return z?parseRanges(z.ranges):[];}
 const o=st.outputs.find(x=>x.id===id);return o?parseRanges(o.id+'1-'+o.count):[];}
function positions(v,pts,n){const W=v.width,H=v.height;if(!pts.length||!n)return [];
 if(pts.length===1||n===1)return Array(n).fill(0).map(()=>({x:pts[0][0],y:pts[0][1]}));
 const seg=[];let total=0;
 for(let i=1;i<pts.length;i++){const d=Math.hypot((pts[i][0]-pts[i-1][0])*W,(pts[i][1]-pts[i-1][1])*H);seg.push(d);total+=d;}
 const out=[];
 for(let k=0;k<n;k++){let t=total*k/(n-1),i=0;while(i<seg.length-1&&t>seg[i]){t-=seg[i];i++;}
  const f=seg[i]?Math.min(1,t/seg[i]):0,a=pts[i],b=pts[i+1];out.push({x:a[0]+(b[0]-a[0])*f,y:a[1]+(b[1]-a[1])*f});}
 return out;}
const photoCv=$('photoCv'),pctx=photoCv.getContext('2d');
function drawPhoto(fi){const v=layout.views[photoView];let im=imgs[v.image];
 if(!im){im=imgs[v.image]=new Image();im.onload=()=>drawLive();im.src=v.image;}
 if(!im.complete||!im.naturalWidth)return;
 const w=$('photoBox').clientWidth,h=Math.round(w*v.height/v.width),dpr=window.devicePixelRatio||1;if(!w)return;
 if(photoCv.width!==Math.round(w*dpr)||photoCv.height!==Math.round(h*dpr)){photoCv.width=Math.round(w*dpr);photoCv.height=Math.round(h*dpr);photoCv.style.height=h+'px';}
 pctx.setTransform(dpr,0,0,dpr,0,0);pctx.globalCompositeOperation='source-over';pctx.drawImage(im,0,0,w,h);
 const set=Object.assign({night:0.6,glow:1},layout.settings||{});
 if(set.night>0){pctx.fillStyle='rgba(0,0,0,'+set.night+')';pctx.fillRect(0,0,w,h);}
 const R=w*0.012*set.glow;pctx.globalCompositeOperation='lighter';
 (v.placements||[]).forEach(p=>{const all=itemLights(p.item),ls=all.slice(Math.max(0,p.from-1),Math.min(all.length,p.to)),pos=positions(v,p.points||[],ls.length);
  pos.forEach((q,i)=>{const c=lightRGB(fi,ls[i][0],ls[i][1]);if(!c)return;const x=q.x*w,y=q.y*h,rgb=c.join(',');
   let g=pctx.createRadialGradient(x,y,0,x,y,R*4);g.addColorStop(0,'rgba('+rgb+',0.22)');g.addColorStop(1,'rgba('+rgb+',0)');
   pctx.fillStyle=g;pctx.beginPath();pctx.arc(x,y,R*4,0,7);pctx.fill();
   g=pctx.createRadialGradient(x,y,0,x,y,R);g.addColorStop(0,'rgba('+rgb+',0.95)');g.addColorStop(0.35,'rgba('+rgb+',0.45)');g.addColorStop(1,'rgba('+rgb+',0)');
   pctx.fillStyle=g;pctx.beginPath();pctx.arc(x,y,R,0,7);pctx.fill();
   pctx.fillStyle='rgba('+c.map(z=>Math.round(z+(255-z)*0.6)).join(',')+',0.9)';pctx.beginPath();pctx.arc(x,y,Math.max(1.2,R*0.16),0,7);pctx.fill();});});
 pctx.globalCompositeOperation='source-over';}

function drawLive(){const hasPhoto=!!(layout&&layout.views&&layout.views.length),usePhoto=liveMode==='photo'&&hasPhoto;
 $('liveMode').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',b.dataset.lm===(usePhoto?'photo':'strip')));
 $('photoBox').hidden=!usePhoto;$('stripBox').hidden=usePhoto;$('photoViews').hidden=!usePhoto;
 $('liveHint').textContent=liveMode==='photo'&&!hasPhoto?'No house photos yet: add them in the House tab. Showing the strip for now.':
  usePhoto?'':'As seen from the street. Screen brightness is approximate.';
 if(!frame)return;const fi=frameInfo(frame);
 if(usePhoto)drawPhoto(fi);else drawStrip(fi);}
window.addEventListener('resize',()=>drawLive());

async function pollFrame(){if(document.hidden||(tab!=='home'&&tab!=='lights')){setTimeout(pollFrame,500);return;}
 const t0=performance.now();
 try{const r=await fetch('/api/frame',{cache:'no-store'});frame=new Uint8Array(await r.arrayBuffer());frames++;drawLive();}catch(e){}
 const now=performance.now();if(now-fpsT>=2000){$('liveInfo').textContent='· '+(frames*1000/(now-fpsT)).toFixed(1)+' updates/s';frames=0;fpsT=now;}
 setTimeout(pollFrame,Math.max(0,100-(now-t0)));}

// ================= Start =================
fetch('/api/state').then(r=>r.json()).then(s=>{show(s);setTab(['home','lights','house','setup'].includes(tab)?tab:'home');})
 .catch(()=>{$('status').textContent='Not connected';setTab('home');});
loadLooksAndScenes();loadLayout();pollFrame();
</script></body></html>)rawhtml";
