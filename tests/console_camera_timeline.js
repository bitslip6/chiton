'use strict';
const assert=require('node:assert/strict'),C=require('../release/files/console-camera-timeline');
const a={target:[-10,20,-30],radius:100,theta:170*Math.PI/180,phi:1};
const b={target:[90,-20,30],radius:400,theta:-170*Math.PI/180,phi:2};
const approx=(x,y)=>assert(Math.abs(x-y)<1e-9,JSON.stringify({x,y}));
const t=new C.CameraTimeline();assert.equal(t.sample(),null);assert.throws(()=>t.play(0));
const source=structuredClone(a),first=t.add(0,source,'perspective');source.target[0]=12345;
const second=t.add(2,b,'perspective');assert.equal(t.count,2);assert.equal(t.duration,2);assert.deepEqual(t.sample(0),a);assert.deepEqual(t.sample(2),b);
const mid=t.sample(1);assert.deepEqual(mid.target,[40,0,0]);approx(mid.radius,200);approx(mid.theta,Math.PI);approx(mid.phi,1.5);
const keys=t.keyframes;keys[0].pose.target[0]=999;keys.pop();assert.deepEqual(t.sample(0),a);assert.equal(t.count,2);
for(const easing of ['linear','smooth','smoother']){
 t.options({easing});const q=t.sample(.5),u=easing==='linear'?.25:easing==='smooth'?.15625:.103515625;
 approx(q.target[0],-10+100*u);approx(q.radius,100*Math.pow(4,u));
 approx(q.theta,a.theta+20*Math.PI/180*u);
}
const before=JSON.stringify(t.keyframes);
for(const time of [-1,3601,NaN,Infinity,'1']){assert.throws(()=>t.add(time,a,'perspective'));assert.throws(()=>t.edit(first,time));assert.throws(()=>t.seek(time));}
for(const pose of [null,{}, {...a,target:[1,2]}, {...a,target:[NaN,0,0]}, {...a,radius:0}, {...a,radius:Infinity}, {...a,theta:1e10}, {...a,phi:0}, {...a,phi:Math.PI}])assert.throws(()=>C.validateCameraPose(pose));
assert.throws(()=>t.add(0,a,'perspective'));assert.throws(()=>t.edit(second,0));assert.throws(()=>t.add(1,a,'orthographic'));
assert.throws(()=>t.options({easing:'bounce'}));assert.throws(()=>t.options({loop:1}));assert.throws(()=>t.remove(999));assert.throws(()=>t.edit(999,1));
assert.equal(JSON.stringify(t.keyframes),before,'failed edits atomic');
t.options({easing:'linear'});t.play(1000);t.advance(1500);approx(t.time,.5);t.advance(1400);approx(t.time,.5);
t.pause(1700);approx(t.time,.7);t.advance(999999);approx(t.time,.7);assert.equal(t.playing,false);
t.play(1000000);t.advance(1000300);approx(t.time,1);t.seek(.25);assert.equal(t.playing,false);approx(t.time,.25);
t.play(0);t.advance(99999);assert.equal(t.time,2);assert.equal(t.playing,false);assert.deepEqual(t.sample(),b);
t.play(1000);assert.equal(t.time,0,'replay from end rewinds');t.pause();
t.options({loop:true});t.play(1000);t.advance(6000);approx(t.time,1);assert.equal(t.playing,true);t.advance(7000);assert.equal(t.time,0);
t.edit(second,3,undefined,7100);assert.equal(t.playing,false);assert.equal(t.duration,3);assert.deepEqual(t.keyframes.find(k=>k.id===second).pose,b);
t.edit(first,4,a);assert.deepEqual(t.keyframes.map(k=>k.id),[second,first],'retiming reorders');
t.remove(first);assert.equal(t.count,1);assert.throws(()=>t.play(0));t.remove(second);assert.equal(t.projection,null);assert.equal(t.duration,0);
const hold=new C.CameraTimeline();hold.add(2,a,'orthographic');hold.add(4,b,'orthographic');assert.deepEqual(hold.sample(0),a);assert.deepEqual(hold.sample(1),a);assert.deepEqual(hold.sample(3600),b);
for(let n=0;n<500;n++){const p=hold.sample(n*4/499);assert(p.target.every(Number.isFinite)&&Number.isFinite(p.radius)&&p.radius>0&&Number.isFinite(p.theta)&&p.phi>0&&p.phi<Math.PI);}
const full=new C.CameraTimeline();for(let n=0;n<C.CAMERA_KEY_LIMIT;n++)full.add(n,a,'perspective');assert.throws(()=>full.add(100,a,'perspective'));assert.equal(full.count,64);
full.clear();assert.equal(full.count,0);assert.equal(full.time,0);assert.equal(full.playing,false);assert.equal(full.projection,null);
console.log('console_camera_timeline: bounded/atomic keyframes, retime/copy, easing, shortest rotation, positive dolly, clock pause/resume/end/loop, holds and finite extrema passed');
