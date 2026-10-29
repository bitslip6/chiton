'use strict';
const assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs');
function run(hz,jitter=false){
    let next=0;const jobs=new Map(),paints=[];
    const context={window:{ChitonConsole:{}},performance:{now:()=>0},requestAnimationFrame:fn=>{const id=++next;jobs.set(id,fn);return id;},cancelAnimationFrame:id=>jobs.delete(id)};
    vm.runInNewContext(fs.readFileSync('release/files/console-cloud-view.js','utf8'),context);
    const view=Object.create(context.window.ChitonConsole.CloudView.prototype);
    Object.assign(view,{active:true,disposed:false,raf:null,lastPaint:-Infinity,render(now){this.lastPaint=now;this.nextPaint=now+1000/30;paints.push(now);}});
    for(let i=0;i<=hz*10;i++){
        view.dirty();assert.equal(jobs.size,1);
        const pending=[...jobs];jobs.clear();const now=i*1000/hz+(jitter?Math.sin(i*1.7)*1.5:0);
        pending.forEach(([,fn])=>fn(now));assert(jobs.size<=1);
    }
    const rate=(paints.length-1)*1000/(paints.at(-1)-paints[0]);
    assert(rate>=29.7&&rate<=30.1,`${hz}Hz jitter=${jitter}: ${rate} FPS`);
    // At most one residual dirty paint; no idle loop, no inactive paint.
    for(const [id,fn] of jobs){jobs.delete(id);fn(11000);}assert.equal(jobs.size,0);
    view.active=false;view.dirty();assert.equal(jobs.size,0);
    return {hz,jitter,rate,paints:paints.length};
}
console.log('console_cloud_cadence:',[run(60),run(120),run(60,true),run(120,true)]);
