// editor_page.h — the House view editor served at "/editor".
//
// Upload photos of the house (shrunk in the browser first), draw a line along the eave
// for each section (a zone, or part of one, or a strand), and its lights are spaced
// evenly along the line. Live mode darkens the photo and draws each light as a glow in
// the real colors from /api/frame.
//
// Layout file (/files/layout.json, written by this page, only stored by the board):
// {
//   "schemaVersion": 1,
//   "settings": {"night": 0.6, "glow": 1.0},
//   "views": [{"id":"v1","name":"Front left","image":"/files/v1-123.jpg","width":1920,"height":1440,
//              "placements":[{"item":"z0","itemName":"Right Side","from":1,"to":26,
//                             "points":[[x,y],...]}]}]     x, y = 0-1 across the photo
// }
// item: "z<slot>" = zone, "A"/"B" = whole strand. from/to: 1-based within the item's lights.
// Lights go from the first point to the last, first and last light on the end points.
//
// A .h on purpose: the Arduino IDE misreads JavaScript inside .ino tabs (see AGENTS.md).

#pragma once

const char EDITOR_HTML[] PROGMEM = R"rawhtml(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>House View</title>
<style>
:root{--bg:#f4f4f2;--card:#fff;--text:#1d1d1b;--muted:#6b6b66;--accent:#d9822b;--line:#deded9;--bad:#c0392b}
@media (prefers-color-scheme:dark){:root{--bg:#141413;--card:#1f1f1d;--text:#ececea;--muted:#9a9a94;--accent:#e8954a;--line:#33332f;--bad:#e06656}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,sans-serif}
main{max-width:1200px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;gap:12px;margin-bottom:12px}
header h1{font-size:22px;margin:0;flex:1}
a{color:var(--accent)}
button,select,input{font:inherit}
.btn{padding:9px 13px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--text);cursor:pointer;font-size:14px}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.btn.danger{color:var(--bad)}
.btn:disabled{opacity:.45;cursor:default}
.seg{display:flex;gap:6px;flex-wrap:wrap}
.seg button{padding:9px 13px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--text);cursor:pointer;font-size:14px}
.seg button.sel{border-color:var(--accent);background:var(--accent);color:#fff}
.bar{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-bottom:10px}
.bar .grow{flex:1}
.stage{position:relative;background:#000;border-radius:12px;overflow:hidden;touch-action:none}
.stage canvas{display:block;width:100%}
.empty{padding:60px 20px;text-align:center;color:#9a9a94}
.layout{display:grid;grid-template-columns:1fr 320px;gap:14px;align-items:start}
@media (max-width:860px){.layout{grid-template-columns:1fr}}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:12px}
.card h2{font-size:15px;margin:0 0 10px}
label{display:block;color:var(--muted);font-size:12px;margin:8px 0 4px}
select,input[type=number],input[type=text]{width:100%;padding:8px;border-radius:9px;border:1px solid var(--line);background:var(--bg);color:var(--text);font-size:15px}
input[type=range]{width:100%;accent-color:var(--accent)}
.row{display:flex;gap:8px;align-items:end}
.row>*{flex:1}
.plist{max-height:280px;overflow:auto}
.pitem{display:flex;gap:8px;align-items:center;padding:7px 8px;border-radius:9px;cursor:pointer;border:1px solid transparent}
.pitem.sel{border-color:var(--accent)}
.pitem div{flex:1;min-width:0}
.pitem small{display:block;color:var(--muted);font-size:12px}
.hint{color:var(--muted);font-size:12px;margin:8px 0 0}
.err{color:var(--bad);font-size:13px;margin:8px 0 0}
.status{color:var(--muted);font-size:13px}
.tag{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:4px}
</style></head><body><main>
<header><h1>House view</h1><span class="status" id="status">Loading…</span><a href="/">← Controls</a></header>

<div class="bar">
<div class="seg" id="views"></div>
<button class="btn" id="addView">+ Add photo</button>
<input type="file" id="fileIn" accept="image/*" hidden>
<span class="grow"></span>
<div class="seg" id="modeSeg"><button data-m="edit">Edit</button><button data-m="live">Live</button></div>
</div>

<div class="layout">
<div>
<div class="stage" id="stage"><canvas id="cv"></canvas><div class="empty" id="empty">Add a photo of the house to start.</div></div>
<p class="hint" id="stageHint"></p>
</div>

<div>
<div class="card" id="liveCard" hidden>
<h2>Night look</h2>
<label>Photo darkness <span id="nightVal"></span></label><input type="range" id="night" min="0" max="0.95" step="0.01">
<label>Glow size <span id="glowVal"></span></label><input type="range" id="glow" min="0.3" max="3" step="0.05">
<p class="hint">Colors come live from the board, about 10 updates a second. Brightness on screen is approximate.</p>
</div>

<div class="card" id="editCard">
<h2>Sections on this photo</h2>
<div class="plist" id="plist"></div>
<p class="hint" id="noP">None yet. Add a section below, then click along the eave on the photo.</p>
<div id="selTools" hidden>
<div class="row" style="margin-top:10px">
<button class="btn" id="drawBtn">Draw line</button><button class="btn" id="undoPt">Undo point</button>
</div>
<div class="row" style="margin-top:8px">
<button class="btn" id="revBtn">Flip direction</button><button class="btn danger" id="delP">Delete section</button>
</div>
<div class="row" style="margin-top:8px">
<div><label>From light</label><input type="number" id="selFrom" min="1"></div>
<div><label>To light</label><input type="number" id="selTo" min="1"></div>
</div>
<p class="hint" id="selInfo"></p>
</div>
</div>

<div class="card" id="addCard">
<h2>Add a section</h2>
<label>Lights from</label><select id="item"></select>
<div class="row">
<div><label>From light</label><input type="number" id="from" min="1" value="1"></div>
<div><label>To light</label><input type="number" id="to" min="1" value="1"></div>
</div>
<p class="hint" id="itemInfo"></p>
<button class="btn primary" id="addP" style="margin-top:10px;width:100%">Add &amp; draw line</button>
<p class="hint">Click on the photo at the <b>first</b> light, add points at corners, end at the <b>last</b> light, then press <b>Finish line</b>. The lights are spaced evenly along the line.</p>
</div>

<div class="card">
<h2>This photo</h2>
<div class="row"><button class="btn" id="renameView">Rename</button><button class="btn danger" id="delView">Delete photo</button></div>
<label style="margin-top:10px"><input type="checkbox" id="tapFind"> Tap a light dot to flash it on the house (Find a light)</label>
<button class="btn" id="stopFind" style="margin-top:6px">Stop flashing</button>
<p class="hint" id="placedInfo"></p>
<p class="err" id="err" hidden></p>
</div>
</div>
</div>
</main><script>
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let layout={schemaVersion:1,settings:{night:0.6,glow:1},views:[]};
let st=null,frame=null,vi=0,mode='edit',selP=-1,drawing=false,drag=null,saveTimer=null;
const img=new Image();let imgFor='';
const cv=$('cv'),ctx=cv.getContext('2d');
const GAM=new Uint8Array(256).map((_,v)=>Math.round(255*Math.pow(v/255,1/2.2)));
try{const m=localStorage.getItem('eaveEditorMode');if(m)mode=m;}catch(e){}

function showErr(m){$('err').textContent=m||'';$('err').hidden=!m;}
function view(){return layout.views[vi];}

// ---- Items: zones and whole strands, as lists of physical lights [output, index] ----
function parseRanges(t){const out=[];
 for(const part of t.split(',')){const m=part.trim().match(/^([AB])(\d+)(?:\s*-\s*[AB]?(\d+))?$/i);if(!m)continue;
  const o=m[1].toUpperCase()==='A'?0:1,a=+m[2]-1,b=(m[3]?+m[3]:+m[2])-1,s=a<=b?1:-1;
  for(let i=a;;i+=s){out.push([o,i]);if(i===b)break;}}
 return out;}
function items(){const list=[];if(!st)return list;
 st.zones.forEach(z=>list.push({id:'z'+z.slot,name:z.name,desc:z.ranges,lights:parseRanges(z.ranges)}));
 list.push({id:'A',name:'Strand A (all)',desc:'A1-65',lights:parseRanges('A1-65')});
 list.push({id:'B',name:'Strand B (all)',desc:'B1-65',lights:parseRanges('B1-65')});
 return list;}
function itemById(id){return items().find(i=>i.id===id);}
function placementLights(p){const it=itemById(p.item);if(!it)return [];
 return it.lights.slice(Math.max(0,p.from-1),Math.min(it.lights.length,p.to));}
function lightLabel(l){return (l[0]===0?'A':'B')+(l[1]+1);}

// ---- Even spacing along the line (in photo pixels, so it isn't stretched) ----
function positions(p,n){const v=view(),W=v.width,H=v.height,pts=p.points;
 if(!pts.length||!n)return [];
 if(pts.length===1||n===1){return Array(n).fill(0).map(()=>({x:pts[0][0],y:pts[0][1]}));}
 const seg=[];let total=0;
 for(let i=1;i<pts.length;i++){const dx=(pts[i][0]-pts[i-1][0])*W,dy=(pts[i][1]-pts[i-1][1])*H,d=Math.hypot(dx,dy);seg.push(d);total+=d;}
 const out=[];
 for(let k=0;k<n;k++){let t=total*k/(n-1),i=0;
  while(i<seg.length-1&&t>seg[i]){t-=seg[i];i++;}
  const f=seg[i]?Math.min(1,t/seg[i]):0,a=pts[i],b=pts[i+1];
  out.push({x:a[0]+(b[0]-a[0])*f,y:a[1]+(b[1]-a[1])*f});}
 return out;}

// ---- Drawing ----
function size(){const v=view();if(!v)return;const w=$('stage').clientWidth,h=Math.round(w*v.height/v.width),dpr=window.devicePixelRatio||1;
 if(cv.width!==Math.round(w*dpr)||cv.height!==Math.round(h*dpr)){cv.width=Math.round(w*dpr);cv.height=Math.round(h*dpr);cv.style.height=h+'px';}
 ctx.setTransform(dpr,0,0,dpr,0,0);return {w,h};}
function lightColor(l){if(!frame)return null;const leds=frame[1]|(frame[2]<<8),k=3+(l[0]*leds+l[1])*3;
 const r=frame[k],g=frame[k+1],b=frame[k+2];return (r|g|b)?[GAM[r],GAM[g],GAM[b]]:null;}
function draw(){const v=view();$('empty').hidden=!!v;cv.hidden=!v;if(!v)return;
 if(imgFor!==v.image){imgFor=v.image;img.onload=()=>draw();img.src=v.image;return;}
 if(!img.complete||!img.naturalWidth)return;
 const {w,h}=size();ctx.globalCompositeOperation='source-over';ctx.drawImage(img,0,0,w,h);
 const live=mode==='live',night=layout.settings.night,glowK=layout.settings.glow;
 if(live&&night>0){ctx.fillStyle='rgba(0,0,0,'+night+')';ctx.fillRect(0,0,w,h);}
 const R=w*0.012*glowK;
 v.placements.forEach((p,pi)=>{const ls=placementLights(p),pos=positions(p,ls.length);
  if(live){ctx.globalCompositeOperation='lighter';
   pos.forEach((q,i)=>{const c=lightColor(ls[i]);if(!c)return;const x=q.x*w,y=q.y*h,rgb=c.join(',');
    // soft wash on the wall around the light
    let g=ctx.createRadialGradient(x,y,0,x,y,R*4);g.addColorStop(0,'rgba('+rgb+',0.22)');g.addColorStop(1,'rgba('+rgb+',0)');
    ctx.fillStyle=g;ctx.beginPath();ctx.arc(x,y,R*4,0,7);ctx.fill();
    // glow
    g=ctx.createRadialGradient(x,y,0,x,y,R);g.addColorStop(0,'rgba('+rgb+',0.95)');g.addColorStop(0.35,'rgba('+rgb+',0.45)');g.addColorStop(1,'rgba('+rgb+',0)');
    ctx.fillStyle=g;ctx.beginPath();ctx.arc(x,y,R,0,7);ctx.fill();
    // hot core
    ctx.fillStyle='rgba('+c.map(x=>Math.round(x+(255-x)*0.6)).join(',')+',0.9)';ctx.beginPath();ctx.arc(x,y,Math.max(1.2,R*0.16),0,7);ctx.fill();});
   ctx.globalCompositeOperation='source-over';return;}
  // edit mode
  const sel=pi===selP;
  if(p.points.length>1){ctx.strokeStyle=sel?'#ffb347':'rgba(255,255,255,0.55)';ctx.lineWidth=sel?2.5:1.5;ctx.beginPath();
   p.points.forEach((q,i)=>i?ctx.lineTo(q[0]*w,q[1]*h):ctx.moveTo(q[0]*w,q[1]*h));ctx.stroke();}
  const dot=Math.max(2.5,Math.min(6,w*0.004));
  pos.forEach((q,i)=>{const c=lightColor(ls[i]),x=q.x*w,y=q.y*h;
   ctx.fillStyle=c?'rgb('+c.join(',')+')':'rgba(20,20,20,0.85)';ctx.beginPath();ctx.arc(x,y,dot,0,7);ctx.fill();
   ctx.lineWidth=i===0?2.5:1;ctx.strokeStyle=i===0?'#3ddc84':'rgba(255,255,255,0.8)';ctx.stroke();});
  if(sel)p.points.forEach(q=>{ctx.fillStyle='#ffb347';ctx.fillRect(q[0]*w-5,q[1]*h-5,10,10);});
  if(p.points.length){const q=p.points[0],it=itemById(p.item);ctx.font='12px system-ui';const t=(it?it.name:p.itemName||'?')+' '+p.from+'–'+p.to;
   const tw=ctx.measureText(t).width;ctx.fillStyle='rgba(0,0,0,0.65)';ctx.fillRect(q[0]*w+8,q[1]*h-22,tw+8,17);ctx.fillStyle='#fff';ctx.fillText(t,q[0]*w+12,q[1]*h-9);}
 });
 if(findLabel){ctx.font='bold 13px system-ui';ctx.fillStyle='#3ddc84';ctx.fillText('Flashing '+findLabel,10,20);}}

// ---- Panels ----
function renderViews(){$('views').innerHTML=layout.views.map((v,i)=>'<button data-i="'+i+'" class="'+(i===vi?'sel':'')+'">'+esc(v.name)+'</button>').join('');
 $('views').querySelectorAll('button').forEach(b=>b.onclick=()=>{vi=+b.dataset.i;selP=-1;drawing=false;renderAll();});}
function renderItems(){const sel=$('item').value;
 $('item').innerHTML=items().map(i=>'<option value="'+i.id+'">'+esc(i.name)+' ('+esc(i.desc)+', '+i.lights.length+')</option>').join('');
 if(sel&&itemById(sel))$('item').value=sel;itemChanged(false);}
function itemChanged(reset){const it=itemById($('item').value);if(!it)return;$('from').max=$('to').max=it.lights.length;
 if(reset!==false||+$('to').value>it.lights.length||+$('to').value<1){$('from').value=1;$('to').value=it.lights.length;}
 $('itemInfo').textContent=it.lights.length+' lights. Use From/To to put only part of it on this photo (e.g. where it wraps a corner).';}
function renderPlist(){const v=view();$('editCard').hidden=$('addCard').hidden=!v||mode!=='edit';$('liveCard').hidden=!v||mode!=='live';
 if(!v){$('placedInfo').textContent='';return;}
 $('plist').innerHTML=v.placements.map((p,i)=>{const it=itemById(p.item),ls=placementLights(p);
  return '<div class="pitem'+(i===selP?' sel':'')+'" data-i="'+i+'"><div>'+esc(it?it.name:(p.itemName||'(deleted zone)'))+
  '<small>lights '+p.from+'–'+p.to+' · '+(ls.length?lightLabel(ls[0])+' → '+lightLabel(ls[ls.length-1]):'none')+' · '+p.points.length+' points</small></div></div>';}).join('');
 $('noP').hidden=v.placements.length>0;
 $('plist').querySelectorAll('.pitem').forEach(d=>d.onclick=()=>{selP=+d.dataset.i;drawing=false;renderAll();});
 const p=v.placements[selP];$('selTools').hidden=!p;
 if(p){$('drawBtn').textContent=drawing?'Finish line':(p.points.length?'Redraw line':'Draw line');$('drawBtn').classList.toggle('primary',drawing);
  $('selFrom').value=p.from;$('selTo').value=p.to;
  $('selInfo').textContent=drawing?'Click on the photo to add points ('+p.points.length+' so far).':'Drag the orange squares to adjust the line.';}
 // count placed lights on all photos
 const placed=new Set();layout.views.forEach(vw=>vw.placements.forEach(p=>placementLights(p).forEach(l=>placed.add(l[0]+':'+l[1]))));
 $('placedInfo').textContent='Placed '+placed.size+' of 130 lights across all photos.';
 $('stageHint').textContent=mode==='edit'?(drawing?'Drawing: click the first light, then corners, then the last light. Press Finish line when done.':'Edit mode: select a section to adjust it.'):'Live: the lights as they are right now.';}
function renderAll(){renderViews();renderPlist();
 $('modeSeg').querySelectorAll('button').forEach(b=>b.classList.toggle('sel',b.dataset.m===mode));
 $('night').value=layout.settings.night;$('nightVal').textContent=Math.round(layout.settings.night*100)+'%';
 $('glow').value=layout.settings.glow;$('glowVal').textContent=layout.settings.glow.toFixed(1)+'×';draw();}

// ---- Saving ----
function save(){clearTimeout(saveTimer);$('status').textContent='Saving…';
 saveTimer=setTimeout(async()=>{try{const r=await fetch('/api/layout',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(layout)});
  const j=await r.json();$('status').textContent=j.error?'Save failed: '+j.error:'Saved';}catch(e){$('status').textContent='Save failed (not connected)';}},600);}

// ---- Photo upload: shrink to max 1920 px, JPEG ----
$('addView').onclick=()=>$('fileIn').click();
$('fileIn').onchange=async e=>{const f=e.target.files[0];e.target.value='';if(!f)return;
 const name=prompt('Name for this photo (e.g. Front left, Center, Front right):',f.name.replace(/\.[^.]+$/,''));if(!name)return;
 $('status').textContent='Shrinking photo…';
 try{const bmp=await createImageBitmap(f);const k=Math.min(1,1920/Math.max(bmp.width,bmp.height));
  const c=document.createElement('canvas');c.width=Math.round(bmp.width*k);c.height=Math.round(bmp.height*k);c.getContext('2d').drawImage(bmp,0,0,c.width,c.height);
  const blob=await new Promise(r=>c.toBlob(r,'image/jpeg',0.85));
  const id='v'+Date.now().toString(36),file=id+'.jpg',fd=new FormData();fd.append('file',blob,file);
  $('status').textContent='Uploading '+Math.round(blob.size/1024)+' KB…';
  const r=await fetch('/api/upload',{method:'POST',body:fd}),j=await r.json();if(j.error)throw new Error(j.error);
  layout.views.push({id,name:name.slice(0,30),image:j.path,width:c.width,height:c.height,placements:[]});
  vi=layout.views.length-1;selP=-1;mode='edit';save();renderAll();}
 catch(err){showErr('Photo upload failed: '+err.message);$('status').textContent='';}};
$('renameView').onclick=()=>{const v=view();if(!v)return;const n=prompt('New name:',v.name);if(n){v.name=n.slice(0,30);save();renderAll();}};
$('delView').onclick=async()=>{const v=view();if(!v||!confirm('Delete photo "'+v.name+'" and its sections?'))return;
 fetch('/api/delete-file',{method:'POST',body:new URLSearchParams({name:v.image.split('/').pop()})}).catch(()=>{});
 layout.views.splice(vi,1);vi=Math.max(0,vi-1);selP=-1;drawing=false;save();renderAll();};

// ---- Sections ----
$('item').onchange=()=>itemChanged(true);
$('addP').onclick=()=>{const v=view(),it=itemById($('item').value);if(!v||!it)return;
 const n=it.lights.length,a=Math.min(n,Math.max(1,+$('from').value||1)),b=Math.min(n,Math.max(a,+$('to').value||n));
 v.placements.push({item:it.id,itemName:it.name,from:a,to:b,points:[]});selP=v.placements.length-1;drawing=true;save();renderAll();};
$('drawBtn').onclick=()=>{const p=view().placements[selP];if(!drawing){p.points=[];drawing=true;}else drawing=false;save();renderAll();};
$('undoPt').onclick=()=>{const p=view().placements[selP];p.points.pop();save();renderAll();};
$('revBtn').onclick=()=>{const p=view().placements[selP];p.points.reverse();save();renderAll();};
$('delP').onclick=()=>{const v=view();if(!confirm('Delete this section?'))return;v.placements.splice(selP,1);selP=-1;drawing=false;save();renderAll();};
function setRange(){const p=view().placements[selP],it=itemById(p.item),n=it?it.lights.length:p.to;
 p.from=Math.min(n,Math.max(1,+$('selFrom').value||1));p.to=Math.min(n,Math.max(p.from,+$('selTo').value||n));save();renderAll();}
$('selFrom').onchange=setRange;$('selTo').onchange=setRange;

// ---- Pointer: add points, drag points, tap a light to find it ----
let findLabel='';
function evPos(e){const r=cv.getBoundingClientRect();return {x:(e.clientX-r.left)/r.width,y:(e.clientY-r.top)/r.height,w:r.width,h:r.height};}
cv.addEventListener('pointerdown',e=>{const v=view();if(!v||mode!=='edit')return;const m=evPos(e),p=v.placements[selP];
 if(drawing&&p){p.points.push([m.x,m.y]);save();renderAll();return;}
 if(p){const i=p.points.findIndex(q=>Math.hypot((q[0]-m.x)*m.w,(q[1]-m.y)*m.h)<10);if(i>=0){drag={p,i};cv.setPointerCapture(e.pointerId);return;}}
 // nearest light dot
 let best=null,bd=12;
 v.placements.forEach((pl,pi)=>{const ls=placementLights(pl);positions(pl,ls.length).forEach((q,k)=>{const d=Math.hypot((q.x-m.x)*m.w,(q.y-m.y)*m.h);if(d<bd){bd=d;best={pi,l:ls[k]};}});});
 if(best){selP=best.pi;if($('tapFind').checked){findLabel=lightLabel(best.l);
   fetch('/api/locate',{method:'POST',body:new URLSearchParams({ranges:findLabel})}).catch(()=>{});}
  renderAll();}});
cv.addEventListener('pointermove',e=>{if(!drag)return;const m=evPos(e);drag.p.points[drag.i]=[Math.min(1,Math.max(0,m.x)),Math.min(1,Math.max(0,m.y))];draw();});
cv.addEventListener('pointerup',()=>{if(drag){drag=null;save();renderAll();}});
$('stopFind').onclick=()=>{findLabel='';fetch('/api/locate',{method:'POST',body:new URLSearchParams({stop:1})}).catch(()=>{});draw();};

// ---- Mode and night look ----
$('modeSeg').querySelectorAll('button').forEach(b=>b.onclick=()=>{mode=b.dataset.m;drawing=false;try{localStorage.setItem('eaveEditorMode',mode);}catch(e){}renderAll();});
$('night').oninput=e=>{layout.settings.night=+e.target.value;$('nightVal').textContent=Math.round(layout.settings.night*100)+'%';draw();save();};
$('glow').oninput=e=>{layout.settings.glow=+e.target.value;$('glowVal').textContent=layout.settings.glow.toFixed(1)+'×';draw();save();};
window.addEventListener('resize',()=>draw());

// ---- Live frames ----
async function pollFrame(){if(document.hidden){setTimeout(pollFrame,500);return;}const t0=performance.now();
 try{const r=await fetch('/api/frame',{cache:'no-store'});frame=new Uint8Array(await r.arrayBuffer());if(!drag)draw();}catch(e){}
 setTimeout(pollFrame,Math.max(0,100-(performance.now()-t0)));}

// ---- Start ----
(async()=>{try{st=await (await fetch('/api/state')).json();}catch(e){$('status').textContent='Not connected';return;}
 if(!st.storage){showErr('Storage not found: photos can\'t be saved. Use Partition Scheme 16M Flash (3MB APP/9.9MB FATFS).');}
 try{const r=await fetch('/files/layout.json',{cache:'no-store'});if(r.ok){const j=await r.json();if(j&&j.views)layout=j;}}catch(e){}
 layout.settings=Object.assign({night:0.6,glow:1},layout.settings||{});
 $('status').textContent=layout.views.length?'Loaded':'No photos yet';renderItems();renderAll();pollFrame();})();
</script></body></html>)rawhtml";
