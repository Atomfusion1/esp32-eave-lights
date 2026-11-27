// Sample data for README screenshots (no real house data). Loaded before the page script.
(function(){
const TAB=(location.hash.slice(1)||'home');
localStorage.setItem('eaveTab',JSON.stringify(TAB));
localStorage.setItem('eaveLiveMode',JSON.stringify('photo'));
localStorage.setItem('eaveSel',JSON.stringify(0));
localStorage.setItem('eaveFolds',JSON.stringify(TAB==='setup'?['strands','radio','power']:[]));
const look=(o)=>Object.assign({on:true,color:'#FF8C28',brightness:200,mode:'solid',fadeSeconds:5,fadeMin:64,speed:20,size:8,reverse:false,random:0,bgOn:true,bothWays:false,flipEvery:6,bgColor:'#1A0A00',colorCount:3,colors:['#FFFFFF','#C81E00','#00A040','#0040FF']},o);
const out=(id,name,type,pin,count,o)=>Object.assign(look(o),{id,name,type,pin,running:true,order:id<'C'?'RBG':'RGB',gpio:type==='wired'?pin:0,enabled:type!=='off',count,cap:{override:id==='D',color:255,sum:765}});
const S={version:'0.41.1',storage:true,canAllOn:false,
 outputs:[out('A','left','wired',8,67,{mode:'bulbs',color:'#C81E00',colorCount:4,colors:['#00A040','#FFFFFF','#1E50FF','#FF8C00'],size:1}),out('B','right','wired',9,65,{mode:'bulbs',color:'#C81E00',colorCount:4,colors:['#00A040','#FFFFFF','#1E50FF','#FF8C00'],size:1}),
  out('C','Palm tree','radio',10,250,{mode:'twinkle',color:'#20C040'}),out('D','Oak floods','radio',11,2,{mode:'alternate',color:'#E01010',colorCount:2,colors:['#10C030','#FFFFFF','#FF0000','#0000FF']}),
  out('E','','off',12,50,{on:false}),out('F','','off',13,50,{on:false})],
 capGlobal:{red:240,green:240,blue:240,sum:570},capMeasured:{color:240,sum:400},restartNeeded:false,pins:[1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,21,38,39,40,41,42,47],
 boardLed:{strand:'D',light:1,pin:48,running:true},
 radio:{ok:true,channel:6,rate:'long',hubs:[{mac:'68:9D:D2:00:00:01',name:'Palm box',fw:'hub 1.1.0',strands:'CD',rssi:-71,rssiHere:-68,pct:99,uptime:3600,age:1}]},
 house:Object.assign(look({on:false,mode:'comet'}),{id:'house',name:'Whole house',length:132}),
 zones:[Object.assign(look({on:false,mode:'solid',color:'#FFB060',colorCount:1}),{id:'z0',slot:0,name:'Porch',ranges:'B1-18',length:18,hidden:false,mirror:null}),
  Object.assign(look({on:false}),{id:'z1',slot:1,name:'Garage',ranges:'A6-23',length:18,hidden:false,mirror:null}),
  Object.assign(look({on:false}),{id:'z2',slot:2,name:'Trunk up',ranges:'C1-30',length:30,hidden:false,mirror:null}),
  Object.assign(look({on:false}),{id:'z3',slot:3,name:'Trunk down',ranges:'C60-31',length:30,hidden:false,mirror:2})],
 paths:[Object.assign(look({on:false,mode:'meteor'}),{id:'p0',slot:0,name:'Rocket',summary:'Trunk up → Fronds',steps:[{zone:2,reverse:false,gap:0}],length:30})],
 scene:{id:6,name:'Christmas classic',edited:false},locate:{active:false,ranges:''}};
// a colorful frame: comet on the eaves, porch warm, palm twinkle, floods orange/purple
function frame(){if(window.DEMO_FRAME)return window.DEMO_FRAME(S.outputs.map(o=>o.count));const n=6,cnt=S.outputs.map(o=>o.count),b=[n];cnt.forEach(c=>b.push(c&255,c>>8));
 const t=Date.now()/1000;
 cnt.forEach((c,o)=>{for(let i=0;i<c;i++){let r=0,g=0,bl=0;
  if(o<2){const head=(t*20)%80,d=head-i;if(d>=0&&d<14){const k=1-d/14;r=255*k;g=110*k;bl=20*k;}else{r=26;g=10;}
   if(o===1&&i<18){r=255;g=170;bl=90;}}
  else if(o===2){const s=Math.sin(i*12.9898)*43758.5;const f=s-Math.floor(s);r=20;g=150+90*f;bl=40;if(f>0.85){r=255;g=255;bl=255;}}
  else if(o===3){if(i===0){r=255;g=90;}else{r=140;bl=255;}}
  b.push(Math.round(r),Math.round(g),Math.round(bl));}});
 return new Uint8Array(b).buffer;}
const scenes=[['Warm white',14],['Falling leaves',6],['Halloween glow',7],['Halloween 3 color',7],['Thanksgiving',8],['Candy cane',9],['Christmas classic',9],['Winter blue',10]]
 .map(([name,group],id)=>({id,name,on:4,colors:['#FF8C28','#C81E00','#20C040'],group}));
const progs={programs:[{id:0,name:'Fall',steps:[{scene:1,seconds:1800},{scene:4,seconds:1800}],useDates:true,from:'10-01',to:'11-30',shuffle:false},
 {id:1,name:'Christmas',steps:[{scene:5,seconds:1200},{scene:6,seconds:1200}],useDates:true,from:'12-01',to:'01-06',shuffle:true}],
 active:{id:1,step:1,next:0,nextIn:900,manual:false},clock:'Wed 7:34 PM',timer:{enabled:true,onMode:0,onClock:1050,onOffset:0,offMode:0,offClock:420,offOffset:0,lat:40,lon:-82.9,sunset:1032,sunrise:461,todayOn:1032,todayOff:461,holdsOff:false,overrideLeft:-1},
 scheduleOn:true,fallback:-1,manual:-1};
window.fetch=async(u)=>{u=String(u);const J=x=>({ok:true,status:200,json:async()=>x,text:async()=>JSON.stringify(x),headers:{get:()=>'0'},arrayBuffer:async()=>frame()});
 if(u.includes('/api/frame')||u.includes('/api/custom?'))return J(null);
 if(u.includes('/api/state')||u.includes('/api/strands')||u.includes('/api/locate'))return J(S);
 if(u.includes('/api/programs'))return J(progs);
 if(u.includes('/api/presets'))return J({presets:[{id:0,name:'Xmas chase',mode:'chase',color:'#C81E00',bgOn:false,bgColor:'#000000',colorCount:2,colors:['#00A040','#FFFFFF','#FF0000','#0000FF']},{id:1,name:'Warm glow',mode:'fade',color:'#FFA040',bgOn:false,bgColor:'#000000',colorCount:1,colors:['#FFFFFF','#FF0000','#00FF00','#0000FF']}],scenes});
 if(u.includes('/api/paint-meta'))return J({paintings:[{id:1,name:'Flag',used:[],locked:false}],swatches:['#FF8C28','#C81E00','#20C040','#FFFFFF']});
 if(u.includes('/api/anims'))return J({anims:[]});
 if(u.includes('/files/layout.json'))return J(window.DEMO_LAYOUT);
 return {ok:false,status:404,json:async()=>({}),text:async()=>''};};
window.addEventListener('error',e=>console.log('PAGEERR '+e.message+' @'+e.lineno));
})();
