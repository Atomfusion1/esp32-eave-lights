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
.swatch{width:34px;height:34px;border-radius:50%;border:2px solid var(--line);cursor:pointer;padding:0}
.swatch.sel{border-color:var(--text);box-shadow:0 0 0 2px var(--accent)}
.swatch.del{border-style:dashed;opacity:.8}
.colorRow{display:flex;gap:8px}
.colorRow input[type=color]{flex:1;min-width:0;height:44px}
.btn{padding:10px 14px;border-radius:10px;border:1px solid var(--line);background:none;color:var(--text);cursor:pointer;font-size:15px}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.btn.danger{color:var(--bad)}
.btn:disabled{opacity:.45;cursor:default}
.row{display:flex;gap:8px;align-items:center}
.row>*{flex:1}
.row .btn{flex:0 0 auto}
.animStep{flex-wrap:wrap;margin-top:6px}.animStep>*{min-width:0}.animStep select,.animStep input[type=number]{padding:8px}.animStep .btn{padding:8px 11px}.animStep label{margin:0;flex:0 0 auto;display:flex;gap:4px;align-items:center}
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
<label>Live <span id="liveInfo"></span> <span class="pill on" id="pvBadge" hidden>PREVIEW · not applied yet</span><span class="pill on" id="paintBadge" hidden></span></label>
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
<div class="row"><select class="picker" id="sceneSel"></select><button class="btn primary" id="sceneShow">Show</button></div>
<div class="chips" id="sceneDots" style="margin-top:8px"></div>
<div class="row" id="sceneTools" style="margin-top:8px"><span class="hint" style="margin:0;flex:0 0 auto">Group</span><select id="sceneGroupSel"></select><button class="btn danger" id="sceneDel">Delete</button></div>
<p class="hint" id="scenesEmpty">No scenes yet. Start from Blank (or any look), set up the house in the Lights tab, then save it here as a scene, e.g. "Xmas".</p>
<div class="row" style="margin-top:12px"><button class="btn" id="saveScene">Save current scene…</button></div>
<div id="sceneForm" class="sub" hidden>
<label>Name</label><input id="sceneName" type="text" maxlength="23" list="sceneNames" placeholder="e.g. Xmas classic"><datalist id="sceneNames"></datalist>
<label style="margin-top:10px">Group</label><select id="sceneNewGroup"></select>
<p class="hint">Saves what every strand, zone and path is doing right now. Using an existing name replaces that scene.</p>
<div class="row"><button class="btn" id="sceneCancel">Cancel</button><button class="btn primary" id="sceneSaveGo">Save scene</button></div>
</div>
<p class="err" id="sceneErr" hidden></p>
</div>
<div class="card">
<h2>Playlists &amp; calendar</h2>
<p class="onList" id="progNow">…</p>
<div class="row" id="progNowBtns" style="margin-top:8px" hidden><button class="btn" id="progNext">Next scene ▶</button><button class="btn danger" id="progStop">Stop</button></div>
<label style="display:flex;gap:8px;align-items:center;margin:14px 0 4px;color:var(--text);font-size:15px"><input type="checkbox" id="schedOn" style="width:20px;height:20px;accent-color:var(--accent)"> Follow the calendar</label>
<p class="hint" id="clockTxt" style="margin:0 0 10px"></p>
<div id="fallbackBox" style="margin:0 0 12px"><label>On days no playlist's dates cover</label>
<select id="fallbackSel"></select>
<p class="hint" id="fallbackHint" style="margin:6px 0 0"></p></div>
<div id="progList"></div>
<p class="hint" id="noProgs">No playlists yet. A playlist rotates through scenes, e.g. Xmas classic 10 min → Snowfall 5 min, and can have dates so it runs by itself every year.</p>
<button class="btn" id="progNew" style="margin-top:10px">+ New playlist</button>
<div id="progForm" class="sub" hidden>
<label id="progFormTitle">New playlist</label>
<input id="progName" type="text" placeholder="Name, e.g. Xmas" maxlength="23">
<label style="display:flex;gap:8px;align-items:center;margin:12px 0 6px;color:var(--text);font-size:14px"><input type="checkbox" id="progDates" style="width:18px;height:18px;accent-color:var(--accent)"> Only between these dates, every year</label>
<div class="row" id="progDateRow" style="flex-wrap:wrap">
<select id="fromM"></select><select id="fromD"></select><span style="flex:0 0 auto">to</span><select id="toM"></select><select id="toD"></select>
</div>
<div class="stepHead" id="progStepHead"><span>Scene (in order)</span><span style="width:84px">Minutes</span><span style="width:84px"></span></div>
<div id="progSteps"></div>
<p class="hint" id="progNeedsScenes" hidden>Save some scenes first (above).</p>
<button class="btn" id="progAddStep" style="margin-top:8px">+ Add scene</button>
<div class="row" style="margin-top:12px"><button class="btn primary" id="progSave">Save playlist</button><button class="btn" id="progCancel">Cancel</button></div>
<p class="err" id="progErr" hidden></p>
</div>
</div>

<div class="card">
<h2>On right now</h2>
<p class="onList" id="onList">…</p>
<button class="power" id="allOff" style="margin-top:12px">All lights off</button>
</div>

<div class="card">
<h2>On / off timer</h2>
<label style="display:flex;gap:8px;align-items:center;margin:0 0 10px;color:var(--text);font-size:15px"><input type="checkbox" id="tmOn" style="width:20px;height:20px;accent-color:var(--accent)"> Turn the lights on and off by themselves</label>
<div id="tmBox">
<label>Turn on at</label>
<div class="row" style="flex-wrap:wrap"><select id="tmOnMode"><option value="0">Sunset</option><option value="1">A set time</option></select>
<input type="time" id="tmOnClock"><span class="hint" style="margin:0;flex:0 0 auto" id="tmOnOffLbl">+</span><input type="number" id="tmOnOffset" min="-180" max="180" step="5" title="Minutes after sunset (negative = before)" style="max-width:90px"><span class="hint" style="margin:0;flex:0 0 auto" id="tmOnMinLbl">min</span></div>
<label style="margin-top:10px">Turn off at</label>
<div class="row" style="flex-wrap:wrap"><select id="tmOffMode"><option value="0">Sunrise</option><option value="1">A set time</option><option value="2">Never (only by hand)</option></select>
<input type="time" id="tmOffClock"><span class="hint" style="margin:0;flex:0 0 auto" id="tmOffOffLbl">+</span><input type="number" id="tmOffOffset" min="-180" max="180" step="5" title="Minutes after sunrise (negative = before)" style="max-width:90px"><span class="hint" style="margin:0;flex:0 0 auto" id="tmOffMinLbl">min</span></div>
<details class="inner"><summary>Location for sunset and sunrise</summary>
<div class="row" style="margin-top:8px"><div><label>Latitude</label><input type="number" id="tmLat" step="0.01"></div><div><label>Longitude</label><input type="number" id="tmLon" step="0.01"></div></div>
<p class="hint">Starts at central Ohio (40.0, -82.9), within a few minutes anywhere in Ohio.</p>
</details>
<div class="row" style="margin-top:12px"><button class="btn primary" id="tmSave">Save timer</button></div>
</div>
<p class="hint" id="tmToday"></p>
<p class="hint">It turns on what was on before "All lights off" (and your calendar playlist, if on). In between you can still switch lights by hand. With the photocell still fitted, the lights only have power at night anyway.</p>
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
<div class="colorRow" id="colorRow"><input id="color" type="color" list="savedColors"></div>
<div class="row" id="colorCountRow" style="margin-top:8px">
<button class="btn" id="colorLess">− color</button><button class="btn" id="colorMore">+ color</button><span class="hint" id="colorCountTxt" style="margin:0"></span>
</div>
</div>
<p class="hint" id="noColorHint" hidden>This effect makes its own colors.</p>
<div id="patBox" hidden style="margin-top:14px">
<label>Painting to show (a painting = every light colored by you; keep up to 30)</label>
<div class="row"><select id="patSel"></select></div>
<div class="row" style="margin-top:8px"><button class="btn" id="patNew">+ New</button><button class="btn" id="patCopy">Copy</button><button class="btn" id="patRename">Rename</button><button class="btn danger" id="patDel">Delete</button></div>
<p class="hint" id="patUsed"></p>
<button class="btn primary" id="paintBtn" style="margin-top:10px;width:100%">✎ Paint it</button>
</div>
<div id="animBox" hidden style="margin-top:14px">
<label>Animation to play (steps that move and fade your colors or paintings; keep up to 30)</label>
<div class="row"><select id="animSel"></select><button class="btn primary" id="animEditBtn">✎ Edit steps</button></div>
<div class="row" style="margin-top:8px"><button class="btn" id="animNew">+ New</button><button class="btn" id="animCopy">Copy</button><button class="btn danger" id="animDel">Delete</button></div>
<p class="hint" id="animSummary"></p>
<p class="hint" id="animUsed"></p>
</div>
<div id="animTools" class="sub" hidden>
<label>Name</label><input id="animName" type="text" maxlength="23">
<div class="row" style="margin-top:10px;align-items:end">
<div style="flex:2"><label>Starts with</label><select id="animStart"></select></div>
<div id="animBandBox"><label>Band width</label><input id="animBand" type="number" min="1" max="30"></div>
</div>
<p class="hint">"Colors" = the effect's color list in bands, side by side. Paintings come from the Custom effect.</p>
<label style="margin-top:10px">Steps (play in order, then loop)</label>
<div id="animSteps"></div>
<div class="row" style="margin-top:8px"><button class="btn" id="animAdd">+ Step</button><button class="btn" id="animDance">+ Dance (forward &amp; back)</button></div>
<p class="hint">Move: + goes along the line (<span id="animDirTxt"></span>), − goes back. Lights that leave one end come back at the other. "Steady" moves at an even speed instead of easing in and out. Loop: <b id="animLoopTxt"></b></p>
<div class="row" style="margin-top:8px"><button class="btn" id="animReset">Reset</button><button class="btn" id="animCancel">Close</button><button class="btn primary" id="animSave">Save</button></div>
<p class="err" id="animErr" hidden></p>
</div>
<div id="paintTools" class="sub" hidden>
<label>Painting <b id="paintNum"></b>: tap or drag over the lights in the live view</label>
<div class="row" style="align-items:center">
<input type="color" id="brush" value="#ff0000" style="height:44px" list="savedColors">
<div class="seg" id="paintTool" style="flex:2"><button data-tool="paint" class="sel">Paint</button><button data-tool="erase">Erase</button><button data-tool="pick" title="Tap a light to take its color (or Shift/Ctrl + click)">Pick</button></div>
</div>
<label style="margin-top:10px">Brush brightness: <span id="brushLvlVal">100%</span> <small>(this light only; the effect's Brightness dims the whole painting on top)</small></label>
<input id="brushLvl" type="range" min="5" max="100" value="100">
<label style="margin-top:10px">Saved colors <span id="swatchMode"></span></label>
<div class="chips" id="swatches"></div>
<div class="row" style="margin-top:8px"><button class="btn" id="swatchAdd">+ Save brush color</button><button class="btn" id="swatchDel">Remove…</button></div>
<div class="row" style="margin-top:8px">
<button class="btn" id="paintFill">Fill with brush</button><button class="btn danger" id="paintClear">Clear</button><button class="btn primary" id="paintDone">Done</button>
</div>
<p class="hint">Only the lights you're controlling can be painted; the rest of the house stays as it is. Shift or Ctrl + click a light (or use Pick) to take its color. With "on Apply", paint shows in the preview and reaches the lights on Apply; Discard throws it away. Black = off, or the background if it's on.</p>
</div>
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
<input id="bgColor" type="color" style="margin-top:10px" list="savedColors" hidden>
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

<details class="fold" data-fold="colors"><summary>Saved colors <small id="savedSum"></small></summary><div class="body">
<p class="hint" style="margin:0 0 10px">These show up as suggestions in every color picker (Chrome/Edge) and as swatches when painting. Up to 12.</p>
<div class="chips" id="savedChips"></div>
<div class="row" style="margin-top:10px;align-items:center"><input type="color" id="savedNew" value="#ff8c3c" style="height:44px"><button class="btn primary" id="savedAdd">Add color</button></div>
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
  else{pending={};pendingTarget=null;stopPreview();await discardPaint();updateApplyBar();}}
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
// paintPending: a painting has unapplied paint (a draft on the board, see markPaintPending).
let applyMode=store.get('eaveApplyMode','apply'),pending={},pendingTarget=null,pendingName='',paintPending=false;
const LOOK_KEYS=['color','brightness','mode','fadeSeconds','fadeMin','speed','size','reverse','bgOn','bgColor','colorCount','color2','color3','color4','color5'];
const hasPending=()=>Object.keys(pending).length>0||paintPending;
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
async function applyPending(){if(!hasPending())return;const look=Object.keys(pending).length>0,p=Object.assign({output:pendingTarget},pending);
 pending={};pendingTarget=null;
 await commitPaint();updateApplyBar();stopPreview();
 try{const j=look?await post('/api/state',p):await (await fetch('/api/state')).json();if(j.error)$('status').textContent=j.error;else show(j);}catch(e){$('status').textContent='Not connected';}}
async function discardPending(){pending={};pendingTarget=null;stopPreview();
 await discardPaint();updateApplyBar();
 try{show(await (await fetch('/api/state')).json());}catch(e){}}
// The board draws unapplied changes into a hidden preview frame; the live view shows it.
let previewTimer=null;
function sendPreview(){clearTimeout(previewTimer);previewTimer=setTimeout(()=>{if(!hasPending())return;
 post('/api/preview',Object.assign({output:pendingTarget},pending)).catch(()=>{});},150);}
function stopPreview(){clearTimeout(previewTimer);post('/api/preview',{stop:1}).catch(()=>{});}
setInterval(()=>{if(hasPending())sendPreview();},60000);   // keep it alive (the board drops it after 3 min)
$('applyBtn').onclick=applyPending;$('discardBtn').onclick=discardPending;
$('applyMode').querySelectorAll('button').forEach(b=>b.onclick=async()=>{applyMode=b.dataset.am;store.set('eaveApplyMode',applyMode);
 if(applyMode==='instant')await applyPending();updateApplyBar();});
window.addEventListener('beforeunload',e=>{if(hasPending()){e.preventDefault();e.returnValue='';}});

async function send(p){if(!st)return;
 if(applyMode==='apply'&&Object.keys(p).every(k=>LOOK_KEYS.includes(k))){
  if(pendingTarget!==target()){pending={};pendingTarget=target();pendingName=curName();}
  Object.assign(pending,p);applyPendingTo(cur(),p);renderLook(cur());updateApplyBar();sendPreview();return;}
 p.output=target();
 try{const j=await post('/api/state',p);if(j.error){$('status').textContent=j.error;}else show(j);}
 catch(e){$('status').textContent='Not connected';}}
function sendSoon(p){clearTimeout(timer);timer=setTimeout(()=>send(p),120);}
// Both: if any strand is on, turn both off; otherwise turn both on.
$('power').onclick=()=>send({on:(both()?anyStrandOn():cur().on)?0:1});
// All lights off, or (when everything is off) back on as it was before.
$('allOff').onclick=async()=>{const on=onNames(st).length>0;
 const j=await post(on?'/api/all-off':'/api/all-on',{});if(j.error)$('status').textContent=j.error;else show(j);};
// oninput while dragging; onchange too, because some browsers only report a picked
// suggestion swatch when the picker closes
$('color').oninput=$('color').onchange=e=>sendSoon({color:e.target.value.slice(1)});
$('bri').oninput=e=>{$('briVal').textContent=pct(e.target.value);sendSoon({brightness:e.target.value});};
$('modeSel').onchange=e=>send({mode:e.target.value});
$('fsec').oninput=e=>{$('fsecVal').textContent=e.target.value+' s';sendSoon({fadeSeconds:e.target.value});};
$('fmin').oninput=e=>{$('fminVal').textContent=pct(e.target.value);sendSoon({fadeMin:e.target.value});};
$('spd').oninput=e=>{const m=modeById(cur().mode),v=+e.target.value;$('spdVal').textContent=m.fmt?m.fmt(v):v;sendSoon({speed:v});};
$('size').oninput=e=>{const m=modeById(cur().mode),v=+e.target.value;$('sizeVal').textContent=m.sizeFmt?m.sizeFmt(v):v+(v===1?' light':' lights');sendSoon({size:v});};
$('dir').querySelectorAll('button').forEach(b=>b.onclick=()=>send({reverse:b.dataset.rev}));
$('bg').querySelectorAll('button').forEach(b=>b.onclick=()=>send({bgOn:b.dataset.bg}));
$('bgColor').oninput=$('bgColor').onchange=e=>sendSoon({bgColor:e.target.value.slice(1)});
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
 {g:'Custom',id:'custom',name:'Custom (paint each light)',colors:0,pattern:1,bg:1,noColorText:'Colors come from the painting.',hint:'Shows a painting: every light colored by hand. Keep up to 8 (Flag, Xmas…); press ✎ Paint to color the lights.'},
 {g:'Custom',id:'anim',name:'Animation (your own steps)',colors:'multi',min:1,anim:1,dir:true,bg:1,hint:'Plays your own steps: move the colors or a painting a few lights, fade between paintings, hold… then loop. Press ✎ Edit steps.'},
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
 $('noColorHint').textContent=m.noColorText||'This effect makes its own colors.';
 // custom: the painting number lives in `size` (1-30)
 $('patBox').hidden=!m.pattern;
 if(m.pattern){const n=Math.min(30,Math.max(1,o.size)),p=paintById(n);
  $('patSel').innerHTML=(p?'':'<option value="'+n+'">(deleted painting)</option>')+paintList.map(q=>'<option value="'+q.id+'">'+(q.locked?'🔒 ':'')+esc(q.name)+'</option>').join('');$('patSel').value=String(n);
  $('paintBtn').textContent=painting?'Painting "'+paintName(paintPattern)+'"…':p&&p.locked?'✎ Paint a copy of "'+p.name+'"':'✎ Paint "'+paintName(n)+'"';
  $('paintBtn').disabled=!p&&!painting;
  $('patUsed').textContent=usedText(p);$('patCopy').disabled=$('patRename').disabled=$('patDel').disabled=!p;}
 else if(painting)stopPainting();
 // animation: the animation number also lives in `size` (1-30)
 $('animBox').hidden=!m.anim;
 if(m.anim){const n=Math.min(30,Math.max(1,o.size)),a=animById(n);
  $('animSel').innerHTML=(a?'':'<option value="'+n+'">(deleted animation)</option>')+animList.map(q=>'<option value="'+q.id+'">'+(q.locked?'🔒 ':'')+esc(q.name)+'</option>').join('');$('animSel').value=String(n);
  $('animSummary').textContent=a?animSummary(a):'';$('animUsed').textContent=usedText(a);
  $('animEditBtn').disabled=!a;$('animCopy').disabled=$('animDel').disabled=!a;
  if(!animEditing)$('animEditBtn').textContent=a&&a.locked?'✎ Edit a copy':'✎ Edit steps';
  if(animEditing&&animEditing.id!==n)closeAnimEditor();}
 else if(animEditing)closeAnimEditor();
 const multi=m.colors==='multi',list=multi?colorsOf(o):[o.color];
 $('colorLabel').textContent=multi?(m.colorNames?m.colorNames.join(' / ')+' colors':'Colors (in order)'):'Color';
 const row=$('colorRow');
 while(row.children.length>list.length)row.removeChild(row.lastChild);
 while(row.children.length<list.length){const k=row.children.length,inp=document.createElement('input');inp.type='color';inp.setAttribute('list','savedColors');
  inp.oninput=inp.onchange=e=>sendSoon({['color'+(k+1)]:e.target.value.slice(1)});row.appendChild(inp);}
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
function showLooks(list){looks=list||[];refreshLocks();
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
  if(pendingTarget===target()){pending={};pendingTarget=null;stopPreview();await discardPaint();}   // the look replaces unapplied changes
  const j=await post('/api/presets',{action:'apply',id:c.dataset.id,output:target()});
  if(j.error)showErr('lookErr',j.error);else{showErr('lookErr');show(j);}});}
$('saveLook').onclick=async()=>{if(hasPending()&&!confirm('Saving uses the settings the lights have now. Your unapplied changes will be applied first. Continue?'))return;
 await applyPending();const name=prompt('Name for this look (e.g. Xmas, Valentines). Saves the current settings of '+curName()+'. Same name replaces it.');
 if(!name)return;const j=await post('/api/presets',{action:'save',name,output:target()});
 if(j.error)showErr('lookErr',j.error);else{showErr('lookErr');showLooks(j.presets);}};

// ================= Scenes =================
// Groups, in the order of the year. The board stores only the id: never renumber.
// 0 = not set yet: shown in the group its name suggests (guessGroup).
const GROUPS=[{id:14,name:'Everyday'},{id:1,name:'New Year\'s'},{id:2,name:'Valentine\'s Day'},{id:3,name:'St. Patrick\'s Day'},
 {id:4,name:'Easter & spring'},{id:5,name:'Patriotic (Memorial Day, July 4th, Labor Day)'},{id:6,name:'Fall'},{id:7,name:'Halloween'},
 {id:8,name:'Thanksgiving'},{id:9,name:'Christmas & holidays'},{id:10,name:'Winter'},{id:11,name:'Party & birthdays'},{id:12,name:'Sports'},{id:13,name:'Other'}];
const GUESSES=[[9,/x-?mas|christmas|santa|noel|holly|candy ?cane|elf|reindeer|hanukk?ah|kwanzaa|nativity|grinch/],[1,/new ?year|nye/],
 [2,/valent|cupid|heart|love/],[3,/patrick|irish|shamrock|clover/],[4,/easter|bunny|spring/],
 [5,/july|4th|fourth|patriot|usa|america|memorial|labor|veteran|flag|red,? ?white/],[7,/hallow|spook|pumpkin|ghost|witch|scary|boo\b|monster/],
 [8,/thanks|turkey|harvest|pilgrim/],[6,/fall|autumn/],[10,/winter|snow|ice|frost/],[11,/birthday|party|b-?day|celebrat/],
 [12,/game|team|sport|football|baseball|hockey|basketball|soccer|buckeye|browns|bengals|guardians/],
 [14,/normal|every|warm|white|default|daily|night|porch|plain|basic/]];
const guessGroup=name=>{const n=String(name).toLowerCase();const g=GUESSES.find(x=>x[1].test(n));return g?g[0]:13;};
const sceneGroupOf=s=>s.group||guessGroup(s.name);
// <option>s of the scenes, grouped by holiday (for every scene drop-down).
function sceneOptions(selId){return GROUPS.map(g=>{const list=sceneList.filter(s=>sceneGroupOf(s)===g.id);
 return list.length?'<optgroup label="'+esc(g.name)+'">'+list.map(s=>'<option value="'+s.id+'"'+(s.id===selId?' selected':'')+'>'+esc(s.name)+'</option>').join('')+'</optgroup>':'';}).join('');}
const groupOptions=()=>GROUPS.map(g=>'<option value="'+g.id+'">'+esc(g.name)+'</option>').join('');
let sceneSelId=store.get('eaveScene','blank');
function showScenes(list){sceneList=list||[];refreshLocks();
 if(sceneSelId!=='blank'&&!sceneList.some(s=>String(s.id)===String(sceneSelId)))sceneSelId='blank';
 $('sceneSel').innerHTML='<optgroup label="Start fresh"><option value="blank">Blank: everything off</option></optgroup>'+sceneOptions(-1);
 $('sceneSel').value=String(sceneSelId);
 $('sceneNames').innerHTML=sceneList.map(s=>'<option value="'+esc(s.name)+'">').join('');
 $('scenesEmpty').hidden=sceneList.length>0;
 renderSceneInfo();
 if(typeof progData!=='undefined'&&progData)showPrograms(progData);}   // playlists show scene names
function renderSceneInfo(){const s=sceneList.find(x=>String(x.id)===String(sceneSelId));
 $('sceneTools').hidden=!s;
 $('sceneDots').innerHTML=s?s.colors.map(c=>'<span class="dot" style="background:'+c+'"></span>').join('')+'<span class="hint" style="margin:0">'+s.on+' parts on</span>'
  :'<span class="hint" style="margin:0">Turns everything off and sets every strand, zone and path back to plain warm white, to build a new look from. Your saved scenes don\'t change.</span>';
 if(s){$('sceneGroupSel').innerHTML=groupOptions();$('sceneGroupSel').value=String(sceneGroupOf(s));}}
$('sceneSel').onchange=e=>{sceneSelId=e.target.value;store.set('eaveScene',sceneSelId);renderSceneInfo();};
$('sceneShow').onclick=async()=>{const blank=sceneSelId==='blank';
 if(blank&&!confirm('Start from Blank? Everything turns off and every strand, zone and path goes back to plain warm white (a playlist that is playing stops). Saved scenes and looks are not changed.'))return;
 if(hasPending())stopPreview();pending={};pendingTarget=null;await discardPaint();   // a scene replaces any unapplied changes
 const j=await post('/api/scenes',blank?{action:'blank'}:{action:'apply',id:sceneSelId});
 if(j.error)showErr('sceneErr',j.error);else{showErr('sceneErr');show(j);if(blank)loadPrograms();}};
$('sceneGroupSel').onchange=async e=>{const j=await post('/api/scenes',{action:'group',id:sceneSelId,group:e.target.value});
 if(j.error)showErr('sceneErr',j.error);else{showErr('sceneErr');showScenes(j.scenes);}};
$('sceneDel').onclick=async()=>{const s=sceneList.find(x=>String(x.id)===String(sceneSelId));if(!s||!confirm('Delete scene "'+s.name+'"? Playlists that use it drop it.'))return;
 const j=await post('/api/scenes',{action:'delete',id:s.id});if(j.error)showErr('sceneErr',j.error);else{showErr('sceneErr');showScenes(j.scenes);loadPrograms();}};
// Save: name + group (the group follows the name until you pick one).
let sceneGroupTouched=false;
$('saveScene').onclick=async()=>{if(hasPending()&&!confirm('Your unapplied changes to '+pendingName+' will be applied first so they are part of the scene. Continue?'))return;
 await applyPending();const s=sceneList.find(x=>String(x.id)===String(sceneSelId));
 $('sceneName').value=s?s.name:'';$('sceneNewGroup').innerHTML=groupOptions();$('sceneNewGroup').value=String(s?sceneGroupOf(s):14);
 sceneGroupTouched=!!s;$('sceneForm').hidden=false;$('sceneName').focus();};
$('sceneName').oninput=()=>{const nm=$('sceneName').value,old=sceneList.find(x=>x.name.toLowerCase()===nm.trim().toLowerCase());
 if(old){$('sceneNewGroup').value=String(sceneGroupOf(old));}else if(!sceneGroupTouched&&nm.trim())$('sceneNewGroup').value=String(guessGroup(nm));};
$('sceneNewGroup').onchange=()=>{sceneGroupTouched=true;};
$('sceneCancel').onclick=()=>{$('sceneForm').hidden=true;};
$('sceneSaveGo').onclick=async()=>{const name=$('sceneName').value.trim();if(!name){showErr('sceneErr','Enter a name for the scene.');return;}
 const old=sceneList.find(x=>x.name.toLowerCase()===name.toLowerCase());
 if(old&&!confirm('Replace scene "'+old.name+'" with what the lights are doing now?'))return;
 const j=await post('/api/scenes',{action:'save',name,group:$('sceneNewGroup').value});
 if(j.error){showErr('sceneErr',j.error);return;}
 showErr('sceneErr');$('sceneForm').hidden=true;
 const saved=(j.scenes||[]).find(x=>x.name.toLowerCase()===name.toLowerCase());if(saved){sceneSelId=String(saved.id);store.set('eaveScene',sceneSelId);}
 showScenes(j.scenes);};
function loadLooksAndScenes(){return fetch('/api/presets').then(r=>r.json()).then(j=>{showLooks(j.presets);showScenes(j.scenes);}).catch(()=>{});}

// ================= Playlists & calendar =================
const MONTHS=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
let progData=null,editProg=-1,progSteps=[];   // progSteps: {scene, minutes} while editing
const sceneName=id=>{const s=sceneList.find(x=>x.id===id);return s?s.name:'(deleted scene)';};
const fmtDate=md=>{const [m,d]=md.split('-').map(Number);return MONTHS[m-1]+' '+d;};
const fmtMins=s=>s%60?(s/60).toFixed(1).replace(/\.0$/,''):String(s/60);
(function fillDateSelects(){const mo=MONTHS.map((n,i)=>'<option value="'+(i+1)+'">'+n+'</option>').join('');
 const dy=Array.from({length:31},(_,i)=>'<option value="'+(i+1)+'">'+(i+1)+'</option>').join('');
 ['fromM','toM'].forEach(id=>$(id).innerHTML=mo);['fromD','toD'].forEach(id=>$(id).innerHTML=dy);})();
function showPrograms(j){progData=j;renderTimer(j.timer);
 $('schedOn').checked=j.scheduleOn;
 // what plays on dates no playlist covers: -1 keep, -2 off, 0+ an everyday playlist
 $('fallbackBox').hidden=!j.scheduleOn;
 $('fallbackSel').innerHTML='<option value="-1">Keep the last look (lights stay as they are)</option><option value="-2">Turn the lights off</option>'+
  j.programs.map(p=>'<option value="'+p.id+'">Play "'+esc(p.name)+'"</option>').join('');
 $('fallbackSel').value=String(j.fallback==null?-1:j.fallback);
 $('fallbackHint').textContent=j.fallback>=0?'Tip: a playlist with one everyday scene (e.g. warm white) works well here.'
  :j.fallback===-2?'When a playlist\'s dates end, the lights go off and stay off until the next playlist\'s dates.'
  :'After e.g. Christmas ends, its look stays on until another playlist\'s dates start.';
 $('clockTxt').textContent=j.clock?'Clock: '+j.clock+(j.stoppedToday?' · calendar paused until tomorrow (Stop was pressed)':''):
  'Clock not set yet (needs internet). Playlists you start with ▶ still work.';
 const a=j.active,ap=a&&j.programs.find(p=>p.id===a.id);
 if(ap){const step=ap.steps[a.step],next=ap.steps[(a.step+1)%ap.steps.length];
  $('progNow').textContent='▶ Playing '+ap.name+(a.manual?'':' (calendar)')+': '+sceneName(step.scene)+
   (ap.steps.length>1?' · next '+sceneName(next.scene)+' in '+Math.ceil(a.nextIn/60)+' min':'');}
 else $('progNow').textContent='No playlist playing.';
 $('progNowBtns').hidden=!ap;$('progNext').hidden=!ap||ap.steps.length<2;
 $('progList').innerHTML=j.programs.map(p=>'<div class="zoneRow"><div>'+(ap&&ap.id===p.id?'▶ ':'')+esc(p.name)+
  '<small>'+(p.useDates?fmtDate(p.from)+' – '+fmtDate(p.to):'no dates (start with ▶)')+' · '+
  p.steps.map(s=>esc(sceneName(s.scene))+' '+fmtMins(s.seconds)+' min').join(' → ')+'</small></div>'+
  '<button class="btn" data-play="'+p.id+'" title="Play now">▶</button><button class="btn" data-pe="'+p.id+'">Edit</button><button class="btn danger" data-pd="'+p.id+'">×</button></div>').join('');
 $('noProgs').hidden=j.programs.length>0;
 $('progList').querySelectorAll('[data-play]').forEach(b=>b.onclick=()=>progAction({action:'play',id:b.dataset.play}));
 $('progList').querySelectorAll('[data-pe]').forEach(b=>b.onclick=()=>startProgEdit(+b.dataset.pe));
 $('progList').querySelectorAll('[data-pd]').forEach(b=>b.onclick=()=>{const p=j.programs.find(x=>x.id===+b.dataset.pd);
  if(confirm('Delete playlist "'+p.name+'"?'))progAction({action:'delete',id:p.id});});}
// ---- On / off timer ----
const hhmm=m=>String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0');
const clock12=m=>{if(m<0)return 'never';const h=Math.floor(m/60),mm=String(m%60).padStart(2,'0');return ((h+11)%12+1)+':'+mm+(h<12?' AM':' PM');};
const fromHHMM=v=>{const [h,m]=String(v||'0:0').split(':').map(Number);return (h||0)*60+(m||0);};
let tmEditing=false;   // don't overwrite the form while the user is changing it
function renderTimer(t){if(!t)return;
 if(!tmEditing){$('tmOn').checked=t.enabled;$('tmOnMode').value=t.onMode;$('tmOnClock').value=hhmm(t.onClock);$('tmOnOffset').value=t.onOffset;
  $('tmOffMode').value=t.offMode;$('tmOffClock').value=hhmm(t.offClock);$('tmOffOffset').value=t.offOffset;$('tmLat').value=t.lat;$('tmLon').value=t.lon;}
 tmFormVisibility();
 $('tmToday').textContent=t.todayOn===undefined?'Clock not set yet (needs internet).':
  'Today: sunset '+clock12(t.sunset)+', sunrise '+clock12(t.sunrise)+(t.enabled?' · lights on '+clock12(t.todayOn)+', off '+clock12(t.todayOff)+(t.holdsOff?' · off by the timer now':''):'');}
function tmFormVisibility(){const onSun=$('tmOnMode').value==='0',offM=$('tmOffMode').value;
 $('tmOnClock').hidden=onSun;['tmOnOffset','tmOnOffLbl','tmOnMinLbl'].forEach(id=>$(id).hidden=!onSun);
 $('tmOffClock').hidden=offM!=='1';['tmOffOffset','tmOffOffLbl','tmOffMinLbl'].forEach(id=>$(id).hidden=offM!=='0');}
['tmOnMode','tmOffMode'].forEach(id=>$(id).addEventListener('change',()=>{tmEditing=true;tmFormVisibility();}));
['tmOnClock','tmOnOffset','tmOffClock','tmOffOffset','tmLat','tmLon'].forEach(id=>$(id).addEventListener('input',()=>tmEditing=true));
function timerFields(){return {enabled:$('tmOn').checked?1:0,onMode:$('tmOnMode').value,onClock:fromHHMM($('tmOnClock').value),onOffset:$('tmOnOffset').value||0,
 offMode:$('tmOffMode').value,offClock:fromHHMM($('tmOffClock').value),offOffset:$('tmOffOffset').value||0,lat:$('tmLat').value,lon:$('tmLon').value};}
$('tmSave').onclick=async()=>{tmEditing=false;await progAction(Object.assign({action:'timer'},timerFields()));};
$('tmOn').onchange=async()=>{tmEditing=false;await progAction(Object.assign({action:'timer'},timerFields()));};

async function progAction(p){const j=await post('/api/programs',p);if(j.error){showErr('progErr',j.error);return null;}
 showErr('progErr');showPrograms(j);try{show(await (await fetch('/api/state')).json());}catch(e){}return j;}
function loadPrograms(){return fetch('/api/programs').then(r=>r.json()).then(showPrograms).catch(()=>{});}
$('schedOn').onchange=e=>progAction({action:'calendar',on:e.target.checked?1:0});
$('fallbackSel').onchange=e=>progAction({action:'calendar',fallback:e.target.value});
$('progNext').onclick=()=>progAction({action:'next'});
$('progStop').onclick=()=>progAction({action:'stop'});
function renderProgSteps(){$('progNeedsScenes').hidden=sceneList.length>0;$('progStepHead').hidden=!progSteps.length;
 $('progDateRow').hidden=!$('progDates').checked;
 $('progSteps').innerHTML=progSteps.map((s,i)=>'<div class="stepRow"><select data-ps="'+i+'">'+
  sceneOptions(s.scene)+
  (sceneList.some(sc=>sc.id===s.scene)?'':'<option selected value="-1">(deleted scene)</option>')+'</select>'+
  '<input type="number" min="0.25" step="0.25" value="'+s.minutes+'" data-pm="'+i+'" style="width:84px;flex:0 0 84px" title="Minutes this scene shows">'+
  '<button class="btn" data-pu="'+i+'" title="Move up"'+(i?'':' disabled')+'>↑</button>'+
  '<button class="btn danger" data-px="'+i+'" title="Remove">×</button></div>').join('');
 const q=a=>$('progSteps').querySelectorAll('['+a+']');
 q('data-ps').forEach(e=>e.onchange=()=>{progSteps[+e.dataset.ps].scene=+e.value;});
 q('data-pm').forEach(e=>e.onchange=()=>{progSteps[+e.dataset.pm].minutes=Math.max(0.25,parseFloat(e.value)||1);});
 q('data-pu').forEach(e=>e.onclick=()=>{const i=+e.dataset.pu;[progSteps[i-1],progSteps[i]]=[progSteps[i],progSteps[i-1]];renderProgSteps();});
 q('data-px').forEach(e=>e.onclick=()=>{progSteps.splice(+e.dataset.px,1);renderProgSteps();});}
function setDateSel(prefix,md){const [m,d]=md.split('-').map(Number);$(prefix+'M').value=m;$(prefix+'D').value=d;}
function openProgForm(title){$('progForm').hidden=false;$('progNew').hidden=true;$('progFormTitle').textContent=title;showErr('progErr');renderProgSteps();}
function closeProgForm(){$('progForm').hidden=true;$('progNew').hidden=false;editProg=-1;progSteps=[];}
$('progNew').onclick=()=>{editProg=-1;$('progName').value='';$('progDates').checked=false;setDateSel('from','12-01');setDateSel('to','12-31');
 progSteps=sceneList.length?[{scene:sceneList[0].id,minutes:10}]:[];openProgForm('New playlist');};
function startProgEdit(id){const p=progData.programs.find(x=>x.id===id);editProg=id;$('progName').value=p.name;$('progDates').checked=p.useDates;
 setDateSel('from',p.from);setDateSel('to',p.to);progSteps=p.steps.map(s=>({scene:s.scene,minutes:+(s.seconds/60).toFixed(2)}));
 openProgForm('Edit playlist: '+p.name);$('progName').focus();}
$('progDates').onchange=renderProgSteps;
$('progAddStep').onclick=()=>{if(!sceneList.length){showErr('progErr','Save some scenes first.');return;}
 const used=progSteps.map(s=>s.scene),next=sceneList.find(s=>!used.includes(s.id))||sceneList[0];
 progSteps.push({scene:next.id,minutes:10});renderProgSteps();};
$('progCancel').onclick=closeProgForm;
$('progSave').onclick=async()=>{const md=p=>String($(p+'M').value).padStart(2,'0')+'-'+String($(p+'D').value).padStart(2,'0');
 const p={action:'save',name:$('progName').value,dates:$('progDates').checked?1:0,from:md('from'),to:md('to'),
  steps:progSteps.map(s=>s.scene+':'+Math.round(s.minutes*60)).join(',')};
 if(editProg>=0)p.id=editProg;if(await progAction(p))closeProgForm();};
// While on Home, refresh what's playing (scenes change by themselves).
setInterval(()=>{if(tab==='home'&&!document.hidden){loadPrograms();
 fetch('/api/state').then(r=>r.json()).then(show).catch(()=>{});}},10000);

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
 const j=await post('/api/zones',{action:'delete',id:slot});if(j.error)showErr('zoneErr',j.error);else{if(editZone===slot)resetZoneForm();show(j);unlinkZoneFromLayout(slot,z.name);}}
// The house photos' sections for a deleted zone are kept (so the drawn line isn't lost) but
// unlinked, so a new zone that later gets the same slot number doesn't take them over.
async function unlinkZoneFromLayout(slot,name){try{
 const r=await fetch('/files/layout.json',{cache:'no-store'});if(!r.ok)return;const lay=await r.json();let n=0;
 (lay.views||[]).forEach(v=>(v.placements||[]).forEach(p=>{if(p.item==='z'+slot){p.item='';p.itemName=name+' (deleted zone)';n++;}}));
 if(!n)return;
 await fetch('/api/layout',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(lay)});
 if(editorFrame.src)editorFrame.src='/editor?embed=1';loadLayout();}catch(e){}}
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
   channels:Object.assign({house:chanFields(s.house)},...s.outputs.map(o=>({[o.id.toLowerCase()]:chanFields(o)}))),
   timer:progData&&progData.timer?(({enabled,onMode,onClock,onOffset,offMode,offClock,offOffset,lat,lon})=>({enabled:enabled?1:0,onMode,onClock,onOffset,offMode,offClock,offOffset,lat,lon}))(progData.timer):null,
   calendar:progData?(progData.scheduleOn?1:0):null,fallback:progData?progData.fallback:null,files};
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
  if(b.timer)await post('/api/programs',Object.assign({action:'timer'},b.timer));
  if(b.calendar!==null&&b.calendar!==undefined)await post('/api/programs',{action:'calendar',on:b.calendar});
  if(b.fallback!==null&&b.fallback!==undefined)await post('/api/programs',{action:'calendar',fallback:b.fallback});
  await loadPrograms();
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
// curBuf = the frame being drawn (live lights, or the pattern while painting).
// dots = where each light was drawn this time, for tapping/painting on the live view.
let curBuf=null,dots=[];
function lightRGB(fi,o,i){if(o>=fi.outs||i>=fi.cnt[o])return null;const k=fi.start[o]+i*3,r=curBuf[k],g=curBuf[k+1],b=curBuf[k+2];
 return (r|g|b)?[GAM[r],GAM[g],GAM[b]]:null;}

// ---- Painting custom patterns ----
let painting=false,paintPattern=1,patternFrame=null,paintTool='paint',paintDown=false,strokeLights=new Set(),strokeTimer=null;
// The lights of what's being controlled, as range text ("A1-67", a zone's ranges, a path's zones).
function targetRangesText(){if(!st)return '';
 const all=()=>st.outputs.map(o=>o.id+'1-'+o.count).join(',');
 if(both()||house())return all();
 if(zoneSel()){const z=cur();return z?z.ranges:'';}
 if(pathSel()){const p=cur();return p?p.steps.map(s=>{const z=zoneBySlot(s.zone);return z?z.ranges:'';}).filter(Boolean).join(','):'';}
 const o=st.outputs[sel];return o?o.id+'1-'+o.count:'';}
// "o:i" keys of those lights: only these can be painted.
function paintTargetSet(){return new Set(parseRanges(targetRangesText()).map(l=>l[0]+':'+l[1]));}
// The live (or preview) frame with the painting shown on the controlled lights only.
// Drawn like the board does (drawCustom): painted colors at the effect's brightness;
// black (unpainted) lights show the background if it's on, otherwise off.
function paintComposite(){const base=frame||patternFrame,out=new Uint8Array(base),fb=frameInfo(base),fp=frameInfo(patternFrame);
 const o0=cur()||{},bri=o0.brightness==null?255:o0.brightness,bgHex=o0.bgOn&&o0.bgColor?o0.bgColor:null;
 const bg=bgHex?[1,3,5].map(k=>parseInt(bgHex.substr(k,2),16)):[0,0,0];
 const lit=v=>v&&bri?Math.max(1,Math.round(v*bri/255)):0;   // like nscale8_video: lit stays lit
 paintTargetSet().forEach(key=>{const [o,i]=key.split(':').map(Number);
  if(o>=fb.outs||o>=fp.outs||i>=fb.cnt[o]||i>=fp.cnt[o])return;
  const a=fb.start[o]+i*3,b=fp.start[o]+i*3;
  let c=[patternFrame[b],patternFrame[b+1],patternFrame[b+2]];if(!(c[0]|c[1]|c[2]))c=bg;
  out[a]=lit(c[0]);out[a+1]=lit(c[1]);out[a+2]=lit(c[2]);});
 return out;}
// "on Apply" mode: paint goes to a draft on the board (only the preview shows it) until Apply / Discard.
const paintDraftArg=()=>(paintPending||applyMode==='apply')?{draft:1}:{};
function markPaintPending(){if(applyMode!=='apply'||paintPending)return;paintPending=true;
 if(!Object.keys(pending).length){pendingTarget=target();pendingName=curName();}
 updateApplyBar();sendPreview();}
async function commitPaint(){if(!paintPending)return;await flushStroke();paintPending=false;
 try{await post('/api/custom',{action:'commit'});}catch(e){$('status').textContent='Paint not saved (not connected)';}}
async function discardPaint(){if(!paintPending)return;paintPending=false;clearTimeout(strokeTimer);strokeLights.clear();
 try{await post('/api/custom',{action:'discard'});}catch(e){}
 if(painting)await startPainting(paintPattern);}
async function startPainting(n){paintPattern=n;
 try{patternFrame=new Uint8Array(await (await fetch('/api/custom?pattern='+n+(paintPending?'&draft=1':''),{cache:'no-store'})).arrayBuffer());}
 catch(e){$('status').textContent='Could not load the pattern';return;}
 painting=true;$('paintTools').hidden=false;$('paintNum').textContent='"'+paintName(n)+'"';
 $('photoCv').style.touchAction='none';$('stripBox').style.touchAction='none';
 if(st)renderLook(cur());drawLive();}
function stopPainting(){flushStroke();painting=false;patternFrame=null;$('paintTools').hidden=true;
 $('photoCv').style.touchAction='';$('stripBox').style.touchAction='';if(st)renderLook(cur());drawLive();}
// The color painted: the brush color at the brush brightness (black for Erase).
function brushHex(){if(paintTool==='erase')return '000000';const lvl=+$('brushLvl').value/100,h=$('brush').value;
 return [1,3,5].map(k=>Math.round(parseInt(h.substr(k,2),16)*lvl).toString(16).padStart(2,'0')).join('');}
$('brushLvl').oninput=()=>{flushStroke();$('brushLvlVal').textContent=$('brushLvl').value+'%';if(paintTool!=='paint')setPaintTool('paint');};
// Sends the lights painted since the last send (batched while dragging).
function flushStroke(){clearTimeout(strokeTimer);if(!strokeLights.size)return Promise.resolve();
 const lights=[...strokeLights].join(',');strokeLights.clear();markPaintPending();
 return post('/api/custom',Object.assign({pattern:paintPattern,lights,color:brushHex()},paintDraftArg())).catch(()=>{$('status').textContent='Paint not saved (not connected)';});}
// The light drawn nearest the pointer, or null.
function nearestDot(box,e){const el=box==='photo'?$('photoCv'):stripCv;if(!el)return null;
 const rc=el.getBoundingClientRect(),x=e.clientX-rc.left,y=e.clientY-rc.top;
 let best=null,bd=1e9;dots.forEach(d=>{if(d.box!==box)return;const dd=Math.hypot(d.x-x,d.y-y);if(dd<=d.r*1.6&&dd<bd){bd=dd;best=d;}});
 return best;}
function setPaintTool(t){paintTool=t;$('paintTool').querySelectorAll('button').forEach(x=>x.classList.toggle('sel',x.dataset.tool===t));renderSwatches();}
// Eyedropper (Shift/Ctrl + click, or the Pick tool): the brush takes that light's color.
// Controlled lights give their painted color; others give what they show now. Black = Erase.
function pickAt(box,e){const best=nearestDot(box,e);if(!best)return;
 const own=patternFrame&&paintTargetSet().has(best.o+':'+best.i),buf=own?patternFrame:(frame||patternFrame);if(!buf)return;
 const fi=frameInfo(buf);if(best.o>=fi.outs||best.i>=fi.cnt[best.o])return;
 const k=fi.start[best.o]+best.i*3,rgb=[buf[k],buf[k+1],buf[k+2]],m=Math.max(...rgb);
 flushStroke();const name=(best.o===0?'A':'B')+(best.i+1);
 if(!m){setPaintTool('erase');$('status').textContent='Picked '+name+': off (Erase)';return;}
 // split into a full-strength color + its brightness, so painting with it matches exactly
 const lvl=Math.max(5,Math.round(m/255*100)),hex='#'+rgb.map(v=>Math.min(255,Math.round(v*255/m)).toString(16).padStart(2,'0')).join('');
 $('brush').value=hex;$('brushLvl').value=lvl;$('brushLvlVal').textContent=lvl+'%';setPaintTool('paint');
 $('status').textContent='Picked '+name+': '+hex.toUpperCase()+' at '+lvl+'%'+(own?'':' (as it shows now)');}
function paintAt(box,e){if(!painting||!patternFrame)return;
 const best=nearestDot(box,e);
 if(!best||!paintTargetSet().has(best.o+':'+best.i))return;   // only the controlled lights
 const fi=frameInfo(patternFrame),k=fi.start[best.o]+best.i*3,hex=brushHex();
 patternFrame[k]=parseInt(hex.slice(0,2),16);patternFrame[k+1]=parseInt(hex.slice(2,4),16);patternFrame[k+2]=parseInt(hex.slice(4,6),16);
 strokeLights.add((best.o===0?'A':'B')+(best.i+1));drawLive();
 clearTimeout(strokeTimer);strokeTimer=setTimeout(flushStroke,150);}
[['photo',$('photoBox')],['strip',$('stripBox')]].forEach(([box,el])=>{
 el.addEventListener('pointerdown',e=>{if(!painting)return;
  if(e.shiftKey||e.ctrlKey||paintTool==='pick'){pickAt(box,e);e.preventDefault();return;}
  paintDown=true;el.setPointerCapture(e.pointerId);paintAt(box,e);e.preventDefault();});
 el.addEventListener('pointermove',e=>{if(painting&&paintDown)paintAt(box,e);});
 el.addEventListener('pointerup',()=>{if(paintDown){paintDown=false;flushStroke();}});
 el.addEventListener('pointercancel',()=>{paintDown=false;flushStroke();});});
// A painting used by a saved scene or look is locked: painting it makes a copy first,
// so saved scenes never change.
$('paintBtn').onclick=async()=>{if(painting){stopPainting();return;}
 const n=Math.min(30,Math.max(1,cur().size)),p=paintById(n);if(!p)return;
 if(p.locked){if(!confirm('"'+p.name+'" is used by '+p.used.join(', ')+'.\nChanging it would change those too.\n\nOK = make a copy and paint the copy.'))return;
  const id=await copyPainting(p.id);if(id)startPainting(id);return;}
 startPainting(n);};
// Copies painting id, shows the copy on what you're controlling. Returns the new id.
async function copyPainting(id){const j=await post('/api/paint-meta',{action:'copy',pattern:id});
 if(j.error){alert(j.error);return 0;}showPaintMeta(j);await send({size:j.newId});return j.newId;}
$('patNew').onclick=async()=>{const name=prompt('Name for the new painting (e.g. Flag, Pumpkins):');if(name===null)return;
 const j=await post('/api/paint-meta',{action:'new',name});if(j.error){alert(j.error);return;}
 showPaintMeta(j);await send({size:j.newId});};
$('patCopy').onclick=()=>copyPainting(+$('patSel').value);
$('patDel').onclick=async()=>{const n=+$('patSel').value;if(!confirm('Delete painting "'+paintName(n)+'"?'))return;
 if(painting&&paintPattern===n)stopPainting();
 const j=await post('/api/paint-meta',{action:'delete',pattern:n});if(j.error){alert(j.error);return;}showPaintMeta(j);};
$('paintDone').onclick=stopPainting;
$('paintTool').querySelectorAll('button').forEach(b=>b.onclick=()=>{flushStroke();paintTool=b.dataset.tool;
 $('paintTool').querySelectorAll('button').forEach(x=>x.classList.toggle('sel',x===b));});
$('brush').oninput=()=>{flushStroke();if(paintTool!=='paint')setPaintTool('paint');};
// Fill / Clear: only the lights of what's being controlled.
async function paintAll(clear){const lights=targetRangesText();if(!lights)return;
 if(!confirm((clear?'Clear the lights of ':'Fill the lights of ')+curName()+(clear?'':' with the brush color')+' in "'+paintName(paintPattern)+'"?'))return;
 await flushStroke();markPaintPending();await post('/api/custom',Object.assign({pattern:paintPattern,lights,color:clear?'000000':brushHex()},paintDraftArg()));
 startPainting(paintPattern);}
$('paintFill').onclick=()=>paintAll(false);$('paintClear').onclick=()=>paintAll(true);

// ---- Painting names and brush swatches (saved on the board) ----
// paintList: the painting library [{id,name,used:["Scene: Xmas",...],locked}] from /api/paint-meta.
let paintList=[],swatchList=[],swatchDelMode=false;
// Saved scenes/looks changed: reload which paintings/animations they lock.
let lockTimer=null;
function refreshLocks(){clearTimeout(lockTimer);lockTimer=setTimeout(()=>{loadPaintMeta();loadAnims();},300);}
// "🔒 Used by Scene: Xmas, …" for a painting or animation.
function usedText(x){if(!x||!x.used||!x.used.length)return '';
 return x.locked?'🔒 Used by '+x.used.join(', ')+'. Editing makes a copy, so those never change.':'Used by '+x.used.join(', ')+'.';}
const paintById=id=>paintList.find(p=>p.id===id);
const paintName=id=>{const p=paintById(id);return p?p.name:'(deleted painting)';};
function showPaintMeta(j){if(!j||j.error)return;paintList=j.paintings||[];swatchList=j.swatches;renderSwatches();if(st)renderLook(cur());}
function loadPaintMeta(){return fetch('/api/paint-meta').then(r=>r.json()).then(showPaintMeta).catch(()=>{});}
// Saved colors also appear as suggestions inside every color picker on the page
// (<datalist> on input type=color: Chrome/Edge show them in the picker popup).
function updateSavedColorList(){let dl=$('savedColors');
 if(!dl){dl=document.createElement('datalist');dl.id='savedColors';document.body.appendChild(dl);}
 dl.innerHTML=swatchList.map(c=>'<option value="'+c.toLowerCase()+'">').join('');}
// Setup > Saved colors: the same list, with × to remove and a picker to add.
function renderSavedSetup(){$('savedSum').textContent=swatchList.length+' of 12';$('savedAdd').disabled=swatchList.length>=12;
 $('savedChips').innerHTML=swatchList.map((c,k)=>'<span class="chip"><span class="dot" style="background:'+c+';width:18px;height:18px"></span>'+c+
  '<span class="x" data-sx="'+k+'" title="Remove">×</span></span>').join('');
 $('savedChips').querySelectorAll('[data-sx]').forEach(x=>x.onclick=()=>saveSwatches(swatchList.filter((_,i)=>i!==+x.dataset.sx)));}
$('savedAdd').onclick=()=>{const c=$('savedNew').value.toUpperCase();if(swatchList.map(x=>x.toUpperCase()).includes(c))return;saveSwatches(swatchList.concat([c]));};
function renderSwatches(){updateSavedColorList();renderSavedSetup();const cur=$('brush').value.toLowerCase();
 $('swatches').innerHTML=swatchList.map((c,k)=>'<button class="swatch'+(swatchDelMode?' del':(c.toLowerCase()===cur&&paintTool==='paint'?' sel':''))+'" data-k="'+k+'" style="background:'+c+'" title="'+(swatchDelMode?'Remove ':'')+c+'"></button>').join('');
 $('swatchMode').textContent=swatchDelMode?'· tap a color to remove it':'· tap to paint with it';
 $('swatchDel').textContent=swatchDelMode?'Done removing':'Remove…';$('swatchAdd').disabled=swatchList.length>=12;
 $('swatches').querySelectorAll('.swatch').forEach(b=>b.onclick=()=>{const k=+b.dataset.k;
  if(swatchDelMode){saveSwatches(swatchList.filter((_,i)=>i!==k));return;}
  flushStroke();$('brush').value=swatchList[k].toLowerCase();paintTool='paint';
  $('paintTool').querySelectorAll('button').forEach(x=>x.classList.toggle('sel',x.dataset.tool==='paint'));renderSwatches();});}
async function saveSwatches(list){showPaintMeta(await post('/api/paint-meta',{action:'swatches',list:list.map(c=>c.replace('#','')).join(',')}));}
$('swatchAdd').onclick=()=>{const c=$('brush').value.toUpperCase();if(swatchList.map(x=>x.toUpperCase()).includes(c))return;saveSwatches(swatchList.concat([c]));};
$('swatchDel').onclick=()=>{swatchDelMode=!swatchDelMode;renderSwatches();};
$('patSel').onchange=async e=>{const n=+e.target.value;
 if(paintPending&&n!==paintPattern){if(confirm('Apply your paint changes to "'+paintName(paintPattern)+'" first?\n\nOK = apply them, Cancel = throw them away.'))await commitPaint();else await discardPaint();updateApplyBar();}
 send({size:n});if(painting)startPainting(n);};
$('patRename').onclick=async()=>{const n=+$('patSel').value,name=prompt('Name for painting '+n+':',paintName(n));
 if(name)showPaintMeta(await post('/api/paint-meta',{action:'rename',pattern:n,name}));};
$('brush').addEventListener('input',renderSwatches);$('brush').addEventListener('change',renderSwatches);

// ================= Animations =================
// Steps text (same as the firmware): h:MS hold, m<L>:MS[s] move, f<F>:MS fade to, c<F>:MS cut to.
// Frame F: 0 = the colors in bands, 1-30 = a painting. Saved animations play at once everywhere.
let animList=[],animEditing=null;   // animEditing: {id,name,start,band,steps:[{k,n,ms,s}]}
const animById=id=>animList.find(a=>a.id===id);
const frameName=f=>f?paintName(f):'Colors';
const frameOptions=()=>'<option value="0">Colors</option>'+paintList.map(p=>'<option value="'+p.id+'">'+esc(p.name)+'</option>').join('');
function parseAnimSteps(t){return (t||'').split(',').filter(x=>x).map(x=>{const m=x.match(/^([hmfc])(-?\d*):(\d+)(s?)$/);
 return m?{k:m[1],n:+(m[2]||0),ms:+m[3],s:!!m[4]}:{k:'h',n:0,ms:1000,s:false};});}
const animStepsText=list=>list.map(s=>s.k+(s.k==='h'?'':s.n)+':'+s.ms+(s.k==='m'&&s.s?'s':'')).join(',');
const secs=ms=>(+(ms/1000).toFixed(2))+' s';
function stepText(s){return s.k==='m'?'move '+(s.n>0?'+':'')+s.n+' over '+secs(s.ms)
 :s.k==='f'?'fade to '+frameName(s.n)+' over '+secs(s.ms)
 :s.k==='c'?'switch to '+frameName(s.n)+(s.ms?', hold '+secs(s.ms):'')
 :'hold '+secs(s.ms);}
function animSummary(a){const list=parseAnimSteps(a.steps);
 return frameName(a.start)+': '+(list.length?list.map(stepText).join(' → ')+', then again':'no steps (stays still)');}
function showAnims(j){if(!j||!j.anims)return;animList=j.anims;if(st)renderLook(cur());}
function loadAnims(){return fetch('/api/anims').then(r=>r.json()).then(showAnims).catch(()=>{});}
function openAnimEditor(n){const a=animById(n);if(!a)return;
 animEditing={id:n,name:a.name,start:a.start,band:a.band,steps:parseAnimSteps(a.steps)};
 $('animTools').hidden=false;$('animEditBtn').textContent='Editing…';showErr('animErr');renderAnimEditor();}
function closeAnimEditor(){animEditing=null;$('animTools').hidden=true;$('animEditBtn').textContent='✎ Edit steps';}
function updateAnimLoop(){const a=animEditing;if(!a)return;
 const total=a.steps.reduce((t,s)=>t+s.ms,0),net=a.steps.reduce((t,s)=>t+(s.k==='m'?s.n:0),0);
 $('animLoopTxt').textContent=secs(total)+(net?', ends '+(net>0?'+':'')+net+' lights along each time round':', ends where it started');
 $('animBandBox').hidden=!(a.start===0||a.steps.some(s=>(s.k==='f'||s.k==='c')&&s.n===0));
 $('animAdd').disabled=$('animDance').disabled=a.steps.length>=16;}
function renderAnimEditor(){const a=animEditing;if(!a)return;
 if(document.activeElement!==$('animName'))$('animName').value=a.name;
 $('animStart').innerHTML=frameOptions();$('animStart').value=String(a.start);$('animBand').value=a.band;
 const selDir=[...$('dir').children].find(b=>b.classList.contains('sel'));
 $('animDirTxt').textContent=selDir?selDir.textContent.split(' (')[0]:'forward';
 $('animSteps').innerHTML=a.steps.length?a.steps.map((s,k)=>'<div class="row animStep" data-k="'+k+'">'+
  '<span class="hint" style="margin:0;flex:0 0 1.4em">'+(k+1)+'.</span>'+
  '<select data-f="k" style="flex:1.3"><option value="m">Move</option><option value="f">Fade to</option><option value="c">Switch to</option><option value="h">Hold</option></select>'+
  (s.k==='m'?'<input data-f="n" type="number" min="-60" max="60" value="'+s.n+'" style="flex:0.8" title="Lights: + along the line, − back">'
   :s.k==='h'?'':'<select data-f="n" style="flex:1.3">'+frameOptions()+'</select>')+
  '<input data-f="ms" type="number" min="0" max="60" step="0.1" value="'+(s.ms/1000)+'" style="flex:0.8" title="Seconds">'+
  '<span class="hint" style="margin:0;flex:0 0 auto">s</span>'+
  (s.k==='m'?'<label><input data-f="s" type="checkbox"'+(s.s?' checked':'')+'>steady</label>':'')+
  '<button class="btn" data-a="up" title="Move this step up"'+(k?'':' disabled')+'>↑</button>'+
  '<button class="btn danger" data-a="del" title="Remove this step">×</button></div>').join('')
  :'<p class="hint">No steps yet: the frame stays still. Add a step below.</p>';
 $('animSteps').querySelectorAll('.animStep').forEach(row=>{const k=+row.dataset.k,s=a.steps[k];
  row.querySelector('[data-f=k]').value=s.k;
  const nSel=row.querySelector('select[data-f=n]');if(nSel)nSel.value=String(s.n);
  row.querySelectorAll('[data-f]').forEach(el=>el.onchange=()=>{const f=el.dataset.f;
   if(f==='k'){if(el.value!==s.k){s.k=el.value;s.n=s.k==='m'?3:s.k==='h'?0:1;s.s=false;}renderAnimEditor();return;}
   if(f==='n')s.n=s.k==='m'?Math.max(-60,Math.min(60,Math.round(+el.value||0))):+el.value;
   if(f==='ms')s.ms=Math.round(Math.max(0,Math.min(60,+el.value||0))*1000);
   if(f==='s')s.s=el.checked;
   updateAnimLoop();});
  row.querySelector('[data-a=up]').onclick=()=>{a.steps.splice(k-1,0,a.steps.splice(k,1)[0]);renderAnimEditor();};
  row.querySelector('[data-a=del]').onclick=()=>{a.steps.splice(k,1);renderAnimEditor();};});
 updateAnimLoop();}
$('animSel').onchange=e=>{const n=+e.target.value;send({size:n});if(animEditing)openAnimEditor(n);};
// Locked (used by a saved scene or look): edit a copy instead.
$('animEditBtn').onclick=async()=>{if(animEditing){closeAnimEditor();return;}
 const a=animById(+$('animSel').value);if(!a)return;
 if(a.locked){if(!confirm('"'+a.name+'" is used by '+a.used.join(', ')+'.\nChanging it would change those too.\n\nOK = make a copy and edit the copy.'))return;
  const id=await copyAnim(a.id);if(id)openAnimEditor(id);return;}
 openAnimEditor(a.id);};
async function copyAnim(id){const j=await post('/api/anims',{action:'copy',id});
 if(j.error){alert(j.error);return 0;}showAnims(j);await send({size:j.newId});return j.newId;}
$('animNew').onclick=async()=>{const name=prompt('Name for the new animation (e.g. Candy dance):');if(name===null)return;
 const j=await post('/api/anims',{action:'new',name});if(j.error){alert(j.error);return;}
 showAnims(j);await send({size:j.newId});openAnimEditor(j.newId);};
$('animCopy').onclick=()=>copyAnim(+$('animSel').value);
$('animDel').onclick=async()=>{const n=+$('animSel').value,a=animById(n);if(!a||!confirm('Delete animation "'+a.name+'"?'))return;
 if(animEditing&&animEditing.id===n)closeAnimEditor();
 const j=await post('/api/anims',{action:'delete',id:n});if(j.error){alert(j.error);return;}showAnims(j);};
$('animName').oninput=e=>{if(animEditing)animEditing.name=e.target.value;};
$('animStart').onchange=e=>{if(animEditing){animEditing.start=+e.target.value;updateAnimLoop();}};
$('animBand').onchange=e=>{if(animEditing){animEditing.band=Math.max(1,Math.min(30,Math.round(+e.target.value||1)));e.target.value=animEditing.band;}};
$('animAdd').onclick=()=>{const a=animEditing;if(a&&a.steps.length<16){a.steps.push({k:'m',n:3,ms:2000,s:false});renderAnimEditor();}};
$('animDance').onclick=()=>{const a=animEditing;if(a&&a.steps.length<=14){a.steps.push({k:'m',n:3,ms:2000,s:false},{k:'m',n:-3,ms:2000,s:false});renderAnimEditor();}};
$('animCancel').onclick=closeAnimEditor;
$('animSave').onclick=async()=>{const a=animEditing;if(!a)return;
 try{const j=await post('/api/anims',{action:'save',id:a.id,name:a.name.trim()||('Animation '+a.id),start:a.start,band:a.band,steps:animStepsText(a.steps)});
  if(j.error){showErr('animErr',j.error);return;}
  showErr('animErr');showAnims(j);$('status').textContent='Animation "'+(animById(a.id)||a).name+'" saved';}
 catch(e){showErr('animErr','Not saved (not connected)');}};
$('animReset').onclick=async()=>{const a=animEditing;if(!a||!confirm('Reset "'+a.name+'" to the default dance?'))return;
 const j=await post('/api/anims',{action:'reset',id:a.id});if(j.error){showErr('animErr',j.error);return;}
 showAnims(j);openAnimEditor(a.id);};
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
  const slot=o===0?cA-1-i:cA+GAP+i,x=step*(slot+0.5);g.beginPath();g.arc(x,h/2,r,0,Math.PI*2);g.fill();
  dots.push({box:'strip',o,i,x,y:h/2,r:Math.max(step*0.6,4)});}}

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
// Evenly spaced light positions along a line; seg = which part of the line (for hidden parts).
function positions(v,pts,n){const W=v.width,H=v.height;if(!pts.length||!n)return [];
 if(pts.length===1||n===1)return Array(n).fill(0).map(()=>({x:pts[0][0],y:pts[0][1],seg:0}));
 const seg=[];let total=0;
 for(let i=1;i<pts.length;i++){const d=Math.hypot((pts[i][0]-pts[i-1][0])*W,(pts[i][1]-pts[i-1][1])*H);seg.push(d);total+=d;}
 const out=[];
 for(let k=0;k<n;k++){let t=total*k/(n-1),i=0;while(i<seg.length-1&&t>seg[i]){t-=seg[i];i++;}
  const f=seg[i]?Math.min(1,t/seg[i]):0,a=pts[i],b=pts[i+1];out.push({x:a[0]+(b[0]-a[0])*f,y:a[1]+(b[1]-a[1])*f,seg:i});}
 return out;}
const photoCv=$('photoCv'),pctx=photoCv.getContext('2d');
function drawPhoto(fi){const v=layout.views[photoView];let im=imgs[v.image];
 if(!im){im=imgs[v.image]=new Image();im.onload=()=>drawLive();im.src=v.image;}
 if(!im.complete||!im.naturalWidth)return;
 const w=$('photoBox').clientWidth,h=Math.round(w*v.height/v.width),dpr=window.devicePixelRatio||1;if(!w)return;
 if(photoCv.width!==Math.round(w*dpr)||photoCv.height!==Math.round(h*dpr)){photoCv.width=Math.round(w*dpr);photoCv.height=Math.round(h*dpr);photoCv.style.height=h+'px';}
 pctx.setTransform(dpr,0,0,dpr,0,0);pctx.globalCompositeOperation='source-over';pctx.drawImage(im,0,0,w,h);
 const set=lookSettings(layout.settings);
 if(set.night>0){pctx.fillStyle='rgba(0,0,0,'+set.night+')';pctx.fillRect(0,0,w,h);}
 const R=w*0.012*set.glow;pctx.globalCompositeOperation='lighter';
 (v.placements||[]).forEach(p=>{const all=itemLights(p.item),ls=all.slice(Math.max(0,p.from-1),Math.min(all.length,p.to)),pos=positions(v,p.points||[],ls.length);
  const hid=p.hidden||[],sp=spacingPx(pos,w,h),pset=p.look?Object.assign({},set,{style:p.look}):set;
  pos.forEach((q,i)=>{if(hid.includes(q.seg))return;   // behind something on this photo
   const c=lightRGB(fi,ls[i][0],ls[i][1]),x=q.x*w,y=q.y*h;
   dots.push({box:'photo',o:ls[i][0],i:ls[i][1],x,y,r:Math.max(Math.min(sp*0.5,R),7)});
   if(!c){if(painting){pctx.globalCompositeOperation='source-over';pctx.strokeStyle='rgba(255,255,255,0.35)';pctx.lineWidth=1;
     pctx.beginPath();pctx.arc(x,y,Math.max(2.5,sp*0.08),0,7);pctx.stroke();pctx.globalCompositeOperation='lighter';}return;}
   drawLight(pctx,x,y,c,sp,R,pset);});});
 pctx.globalCompositeOperation='source-over';}

// ---- How a light looks on the photo (same code in the House editor) ----
function lookSettings(s){return Object.assign({night:0.6,glow:1,style:'down',throw:1,spread:1},s||{});}
// Average distance between neighbouring lights of a section, in canvas px.
function spacingPx(pos,w,h){if(pos.length<2)return w*0.04;let d=0;
 for(let k=1;k<pos.length;k++)d+=Math.hypot((pos[k].x-pos[k-1].x)*w,(pos[k].y-pos[k-1].y)*h);return Math.max(4,d/(pos.length-1));}
// Downlight (default): like the real eave pucks, a small bright point with a scallop of
// light fanning down the wall below it and a faint wash. Glow: a round glow (older look).
// Draw with globalCompositeOperation 'lighter'. c = [r,g,b] already gamma-lifted.
function drawLight(g,x,y,c,sp,R,set){const rgb=c.join(','),core='rgba('+c.map(z=>Math.round(z+(255-z)*0.7)).join(',')+',0.95)';
 // Points only: the puck with a small halo (eaves seen side-on, e.g. up the roof line)
 if(set.style==='point'){const r=Math.max(3,sp*0.3);
  const gr=g.createRadialGradient(x,y,0,x,y,r);gr.addColorStop(0,'rgba('+rgb+',0.8)');gr.addColorStop(1,'rgba('+rgb+',0)');
  g.fillStyle=gr;g.beginPath();g.arc(x,y,r,0,7);g.fill();
  g.fillStyle=core;g.beginPath();g.arc(x,y,Math.max(1.2,sp*0.07),0,7);g.fill();return;}
 if(set.style==='glow'){
  let gr=g.createRadialGradient(x,y,0,x,y,R*4);gr.addColorStop(0,'rgba('+rgb+',0.22)');gr.addColorStop(1,'rgba('+rgb+',0)');
  g.fillStyle=gr;g.beginPath();g.arc(x,y,R*4,0,7);g.fill();
  gr=g.createRadialGradient(x,y,0,x,y,R);gr.addColorStop(0,'rgba('+rgb+',0.95)');gr.addColorStop(0.35,'rgba('+rgb+',0.45)');gr.addColorStop(1,'rgba('+rgb+',0)');
  g.fillStyle=gr;g.beginPath();g.arc(x,y,R,0,7);g.fill();
  g.fillStyle=core;g.beginPath();g.arc(x,y,Math.max(1.2,R*0.16),0,7);g.fill();return;}
 const L=Math.max(6,sp*set.throw),W=Math.max(3,sp*0.5*set.spread);
 // faint wash on the wall around the light
 let gr=g.createRadialGradient(x,y+L*0.35,0,x,y+L*0.35,L*1.4);gr.addColorStop(0,'rgba('+rgb+',0.10)');gr.addColorStop(1,'rgba('+rgb+',0)');
 g.fillStyle=gr;g.beginPath();g.arc(x,y+L*0.35,L*1.4,0,7);g.fill();
 // the scallop: an oval of light below the puck, cut off at the eave line
 g.save();g.beginPath();g.rect(x-W*2,y,W*4,L*1.3);g.clip();
 g.translate(x,y);g.scale(W/L,1);
 gr=g.createRadialGradient(0,L*0.1,0,0,L*0.1,L);
 gr.addColorStop(0,'rgba('+rgb+',0.75)');gr.addColorStop(0.35,'rgba('+rgb+',0.42)');gr.addColorStop(0.75,'rgba('+rgb+',0.12)');gr.addColorStop(1,'rgba('+rgb+',0)');
 g.fillStyle=gr;g.beginPath();g.arc(0,L*0.1,L,0,7);g.fill();g.restore();
 // the puck itself
 g.fillStyle=core;g.beginPath();g.arc(x,y,Math.max(1.2,sp*0.07),0,7);g.fill();}

function drawLive(){const hasPhoto=!!(layout&&layout.views&&layout.views.length),usePhoto=liveMode==='photo'&&hasPhoto;
 $('liveMode').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',b.dataset.lm===(usePhoto?'photo':'strip')));
 $('photoBox').hidden=!usePhoto;$('stripBox').hidden=usePhoto;$('photoViews').hidden=!usePhoto;
 $('liveHint').textContent=liveMode==='photo'&&!hasPhoto?'No house photos yet: add them in the House tab. Showing the strip for now.':
  usePhoto?'':'As seen from the street. Screen brightness is approximate.';
 // while painting: the controlled lights show the painting, everything else stays live
 const src=painting&&patternFrame?paintComposite():frame;
 $('paintBadge').hidden=!painting;$('paintBadge').textContent='PAINTING PATTERN '+paintPattern;
 if(!src)return;curBuf=src;dots=[];const fi=frameInfo(src);
 if(usePhoto)drawPhoto(fi);else drawStrip(fi);}
window.addEventListener('resize',()=>drawLive());

async function pollFrame(){if(document.hidden||(tab!=='home'&&tab!=='lights')){setTimeout(pollFrame,500);return;}
 const t0=performance.now();
 try{const r=await fetch('/api/frame'+(hasPending()?'?preview=1':''),{cache:'no-store'});
  $('pvBadge').hidden=r.headers.get('X-Preview')!=='1';
  frame=new Uint8Array(await r.arrayBuffer());frames++;drawLive();}catch(e){}
 const now=performance.now();if(now-fpsT>=2000){$('liveInfo').textContent='· '+(frames*1000/(now-fpsT)).toFixed(1)+' updates/s';frames=0;fpsT=now;}
 setTimeout(pollFrame,Math.max(0,100-(now-t0)));}

// ================= Start =================
fetch('/api/state').then(r=>r.json()).then(s=>{show(s);setTab(['home','lights','house','setup'].includes(tab)?tab:'home');})
 .catch(()=>{$('status').textContent='Not connected';setTab('home');});
loadLooksAndScenes().then(loadPrograms);loadPaintMeta();loadAnims();loadLayout();pollFrame();
</script></body></html>)rawhtml";
