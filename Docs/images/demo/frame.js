// Christmas frame for the screenshots (overrides frame() in mock2.js through window.DEMO_FRAME).
window.DEMO_FRAME=function(cnt){const b=[cnt.length];cnt.forEach(c=>b.push(c&255,c>>8));
 const bulbs=[[230,20,10],[10,190,50],[255,240,210],[30,80,255],[255,140,0]];
 const rnd=i=>{const s=Math.sin(i*12.9898+78.233)*43758.5453;return s-Math.floor(s);};
 cnt.forEach((c,o)=>{for(let i=0;i<c;i++){let col=[0,0,0];
  if(o<2){col=bulbs[(i+o*3)%5].map(v=>Math.round(v*(0.75+0.25*rnd(i*7+o))));}
  else if(o===2){const f=rnd(i);col=i<60?[180,120,30]:[20,140+Math.round(80*f),40];if(f>0.88)col=[255,230,140];}
  else if(o===3){col=i===0?[230,20,20]:[20,200,50];}
  b.push(...col);}});return new Uint8Array(b).buffer;};
