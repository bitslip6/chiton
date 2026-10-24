'use strict';
const assert=require('node:assert/strict'),C=require('../release/files/console-cloud'),{Scheduler}=require('../release/files/console-scheduler');
class Clock{
    constructor(){this.now=0;this.id=0;this.jobs=new Map();}
    set=(fn,ms)=>{const id=++this.id;this.jobs.set(id,{fn,time:this.now+ms});return id;};clear=id=>this.jobs.delete(id);
    async flush(){for(let i=0;i<16;i++)await Promise.resolve();}
    async advance(ms){const end=this.now+ms;for(let n=0;n<10000;n++){const first=[...this.jobs].sort((a,b)=>a[1].time-b[1].time)[0];if(!first||first[1].time>end){this.now=end;await this.flush();return;}this.now=first[1].time;this.jobs.delete(first[0]);first[1].fn();await this.flush();}throw new Error('Timer loop');}
}
(async()=>{
    const tracker=new C.CloudTracker(),snapshot=(id,revision=0,age=0,received=0)=>({source:'live',frameId:id,revision,frameType:'depth',units:'mm',unitsScope:'calibration_aoi',frameAgeMs:age,receivedMonoMs:received,recordCount:1,bytes:8,records:new Int16Array(4)});
    assert(tracker.accept(snapshot(2),{},0).reset);assert(tracker.accept(snapshot(2,0,0,500),{},500).duplicate);
    assert.equal(tracker.times.length,1);assert.match(tracker.health(1600,2),/stale/);assert(tracker.accept(snapshot(1),{},1600).ignored);
    tracker.reconnecting=true;assert(tracker.accept(snapshot(1,0,0,1700),{},1700).reset);assert.equal(tracker.latest.frameId,1);
    assert(tracker.accept(snapshot(1,1,0,1800),{},1800).reset);assert.equal(tracker.times.length,1);
    tracker.latest.recordCount=0;assert.match(tracker.health(1800,2),/Empty cloud/);tracker.latest.frameAgeMs=null;assert.match(tracker.health(1900,2),/live/);
    for(const hz of [1,2,5,10]){
        const c=new Clock(),starts=[];let active=0,max=0,points=0,images=0;
        const common={clock:()=>c.now,setTimer:c.set,clearTimer:c.clear};
        const cloud=new Scheduler({...common,hz,read:signal=>new Promise((resolve,reject)=>{starts.push(c.now);max=Math.max(max,++active);const t=c.set(()=>{active--;resolve(1);},100);signal.addEventListener('abort',()=>{c.clear(t);active--;reject(new Error('cancel'));},{once:true});})});
        const point=new Scheduler({...common,hz:20,read:async()=>points++}),image=new Scheduler({...common,hz:30,read:async()=>images++});
        cloud.start();point.start();image.start();await c.advance(1900);assert.equal(max,1);for(let i=1;i<starts.length;i++)assert.equal(starts[i]-starts[i-1],1000/hz);assert.equal(points,39);assert(images>=57);
        cloud.stop();point.stop();image.stop();await c.flush();assert.equal(c.jobs.size,0);
    }
    // Shared Scheduler owns deadline/capped backoff/generation logic: run its full regression too.
    require('./console_scheduler');console.log('console_cloud_scheduler: independent 1/2/5/10 Hz, one flight, frozen/duplicate/order/restart/context/empty states passed');
})().catch(e=>{console.error(e);process.exitCode=1;});
