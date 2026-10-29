'use strict';
const assert=require('node:assert/strict');
const {Kalman,presets}=require('../release/files/console-kalman');const {stats}=require('../release/files/console-model');const {pixel}=require('../release/files/console-demo');
let k=new Kalman({R:25,q:125});assert.equal(k.update(100,1000,1),100);assert.equal(k.P,25);
const gain=31.25/56.25;assert.equal(k.update(110,1050,2),100+gain*10);assert.equal(k.P,(1-gain)*31.25);
const unchanged=k.x;assert.equal(k.update(500,1100,2),unchanged);assert.equal(k.update(500,1100,1),unchanged);
assert.equal(k.update(0,4000,3),0);assert.equal(k.update(65535,3900,4),65535);assert.match(k.reason,/discontinuity/);
assert.equal(k.update(100,null,5),null);assert.equal(k.update(100,5000,6),100);
for(const p of [{R:0,q:1},{R:Infinity,q:1},{R:25,q:-1},{R:25,q:1e10}])assert.throws(()=>new Kalman(p));
k=new Kalman(presets.Balanced);const measured=[],filtered=[];
for(let i=1;i<=600;i++){const z=pixel(460,320,i,{seed:7});const estimate=k.update(z,1000+i*50,i);if(i>100){measured.push(z);filtered.push(estimate);}}
const ratio=stats(filtered).stddev/stats(measured).stddev;assert(ratio<.7);
k=new Kalman(presets.Balanced);for(let i=1;i<=60;i++)k.update(1000,1000+i*50,i);let settling=null;
for(let i=61;i<=100;i++){k.update(1500,1000+i*50,i);if(settling==null&&k.x>=1450)settling=(i-60)*.05;}
assert(settling<=1);assert(k.x>1499);
k=new Kalman(presets.Balanced);k.update(1000,1000,1);assert(k.update(2000,1050,2)<2000);assert(k.update(1000,1100,3)>1000);
console.log(JSON.stringify({test:'console_kalman',stationaryStddevRatio:ratio,step90PercentSeconds:settling,note:'synthetic tuning only'}));
