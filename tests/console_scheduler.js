'use strict';
const assert=require('node:assert/strict');const {Scheduler}=require('../release/files/console-scheduler');
class Clock{
    constructor(){this.now=0;this.id=0;this.jobs=new Map();}
    set=(fn,ms)=>{const id=++this.id;this.jobs.set(id,{fn,time:this.now+ms});return id;};
    clear=id=>this.jobs.delete(id);
    async flush(){for(let i=0;i<12;i++)await Promise.resolve();}
    async advance(ms){const end=this.now+ms;for(let n=0;n<10000;n++){const first=[...this.jobs].sort((a,b)=>a[1].time-b[1].time)[0];if(!first||first[1].time>end){this.now=end;await this.flush();return;}this.now=first[1].time;this.jobs.delete(first[0]);first[1].fn();await this.flush();}throw new Error('Timer loop');}
}
(async()=>{
    for(const hz of [5,10,20,30]){
        const c=new Clock(),starts=[];let active=0,max=0;
        const loop=new Scheduler({hz,clock:()=>c.now,setTimer:c.set,clearTimer:c.clear,read:signal=>new Promise((resolve,reject)=>{
            starts.push(c.now);max=Math.max(max,++active);const t=c.set(()=>{active--;resolve(1);},10);signal.addEventListener('abort',()=>{c.clear(t);active--;reject(new Error('Aborted'));},{once:true});
        })});loop.start();await c.advance(990);assert.equal(max,1);for(let i=1;i<starts.length;i++)assert(Math.abs(starts[i]-starts[i-1]-1000/hz)<1e-6);loop.stop();await c.flush();assert.equal(c.jobs.size,0);
    }
    const c=new Clock(),starts=[],delivered=[];let fail=true,delay=0;
    const loop=new Scheduler({clock:()=>c.now,setTimer:c.set,clearTimer:c.clear,deadline:2000,read:signal=>new Promise((resolve,reject)=>{
        starts.push(c.now);const timer=c.set(()=>fail?reject(new Error('fail')):resolve(starts.length),delay);
        signal.addEventListener('abort',()=>{c.clear(timer);reject(new Error('abort'));},{once:true});
    }),onValue:v=>delivered.push(v)});
    loop.start();await c.advance(15500);assert.deepEqual(starts,[0,500,1500,3500,7500,15500]);
    fail=false;await c.advance(8000);assert.equal(loop.failures,0);await c.advance(50);assert.equal(starts.at(-1),23550);
    delay=180;loop.restart(20);await c.advance(500);assert(starts.at(-1)-starts.at(-2)>=180); // no catch-up bursts
    delay=5000;loop.restart();await c.advance(2100);assert.equal(loop.failures,1); // deadline covers whole promise/body
    const old=delivered.length;loop.stop();await c.advance(10000);assert.equal(delivered.length,old);assert.equal(c.jobs.size,0);
    loop.start();delay=0;await c.advance(0);loop.restart(10);await c.advance(100);loop.stop();await c.flush();assert.equal(c.jobs.size,0);
    console.log('console_scheduler: cadence, slow reads, single-flight, deadlines, cap/reset backoff, generations, pause/resume passed');
})().catch(e=>{console.error(e);process.exitCode=1;});
