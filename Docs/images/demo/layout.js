// Sample house layout for the screenshots: eaves A (left of the controller) and B (right),
// palm trunk + round top (C), two floods at the oak (D). Coordinates 0-1 of house.png.
(function(){
const W=1600,H=900,P=(x,y)=>[+(x/W).toFixed(4),+(y/H).toFixed(4)];
const trunk=[];for(let k=0,y=835;y>=560;y-=18,k++)trunk.push(P(k%2?1512:1490,y));
const top=[];const cx=1503,cy=505;for(let a=0;a<=360*4;a+=12){const r=82-a/360*16,t=a*Math.PI/180;top.push(P(cx+r*Math.cos(t),cy+r*Math.sin(t)*0.95));}
window.DEMO_LAYOUT={schemaVersion:1,settings:{night:0.25,glow:1.3,style:'down',throw:1.0,spread:1.0},
 views:[{id:'v1',name:'Front',image:'house.png',width:W,height:H,placements:[
  {item:'A',itemName:'Strand A',from:1,to:67,points:[P(770,447),P(195,447)]},
  {item:'B',itemName:'Strand B',from:1,to:65,points:[P(810,447),P(1415,447)]},
  {item:'C',itemName:'Palm trunk',from:1,to:60,points:trunk,look:'point'},
  {item:'C',itemName:'Palm top',from:61,to:250,points:top,look:'point'},
  {item:'D',itemName:'Flood 1',from:1,to:1,points:[P(90,860)],look:'glow'},
  {item:'D',itemName:'Flood 2',from:2,to:2,points:[P(260,870)],look:'glow'}]}]};
})();
