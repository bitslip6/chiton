/* Camera-only framing of signed records, not sensor validity filtering. */
'use strict';
const assert=require('node:assert/strict'),C=require('../release/files/console-cloud');
for(const n of [0,1,99,100,1000,307200]){
    const a=new Int16Array(n*4);
    for(let i=0;i<n;i++)a.set([i%10-5,(i%7)-3,i%13,500],i*4);
    if(n>=100)a.set([32767,-32768,32767,-32768],(n-1)*4);
    const original=Buffer.from(a.buffer).toString('hex'),all=C.cloudBounds(a),main=C.cloudMainBounds(a);
    assert.equal(Buffer.from(a.buffer).toString('hex'),original);assert.equal(main.total,n);
    assert(main.included<=n&&main.included>=Math.floor(n*.94));
    assert(main.center.every(Number.isFinite)&&Number.isFinite(main.radius)&&main.radius>=1);
    if(n<100){assert.deepEqual(main.center,all.center);assert.equal(main.radius,all.radius);}
    else {assert(main.radius<all.radius/10);assert.equal(main.included,n-1);}
    const nav=new C.CloudCamera();nav.bounds=all;nav.orbit(.3,.2);const orientation=[nav.theta,nav.phi];
    nav.fit(1,main);assert.deepEqual([nav.theta,nav.phi],orientation);assert.deepEqual(nav.target,main.center);
    const target=[...nav.target];nav.pan(1,1);
    assert(Math.hypot(...nav.target.map((v,i)=>v-target[i]))<nav.base*.01,'small pan after main fit must not jump to all-record center');
    for(const view of ['front','top','side']){nav.orient(view);assert(nav.position().every(Number.isFinite));}
    assert.throws(()=>nav.orient('invalid'));nav.home();assert.deepEqual(nav.target,all.center);
}
const same=new Int16Array(1000*4);assert.equal(C.cloudMainBounds(same).radius,1);
assert.throws(()=>C.cloudMainBounds(new Float32Array(4)));
console.log('console_cloud_framing: empty/small/max/signed outlier robust framing, preserved records/orientation, API presets and fit-all reset passed');
