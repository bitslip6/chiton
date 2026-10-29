/* Dependency-free clipping contracts; no sensor/browser required. */
'use strict';
const assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs');
const C=require('../release/files/console-cloud');
const defaults=C.defaultClipSettings(),copy=C.validateClipSettings(defaults);copy.box.min[0]=0;assert.equal(defaults.box.min[0],-32768);
const bad=[null,{},[],{depth:defaults.depth},...[
 s=>delete s.depth.enabled,s=>s.depth.enabled=1,s=>s.box.enabled='true',s=>delete s.box.min,s=>s.box.min=[0,1],s=>s.box.max=[1,2,3,4],
 ...[NaN,Infinity,-Infinity,1.5,-32769,32768,null,'1'].map(v=>s=>s.depth.min=v),s=>s.depth.min=32767,s=>s.box.max[1]=-32769,s=>s.box.min[0]=32767
].map(change=>{const s=C.defaultClipSettings();change(s);if(s.depth.min===32767)s.depth.max=0;if(s.box.min?.[0]===32767)s.box.max[0]=0;return s;})];
for(const state of bad)assert.throws(()=>C.validateClipSettings(state));
const records=new Int16Array([-32768,-32768,-32768,-32768, 0,0,0,0, 32767,32767,32767,32767, -2,3,4,-1, 1,2,-7,4]);
const original=[...records];
assert.deepEqual(C.summarizeCloudClipping(records,defaults),{matchingCount:5,clippedCount:0,submittedCount:5});
const s=C.defaultClipSettings();s.depth={enabled:true,min:-1,max:0};
assert.equal(C.summarizeCloudClipping(records,s).matchingCount,2);
s.box={enabled:true,min:[0,0,0],max:[0,0,0]};assert.equal(C.summarizeCloudClipping(records,s).matchingCount,1);
s.depth.min=s.depth.max=4;assert.equal(C.summarizeCloudClipping(records,s).matchingCount,0);s.box.enabled=false;assert.equal(C.summarizeCloudClipping(records,s).matchingCount,1);
for(const d of [-32768,0,32767]){s.depth.min=s.depth.max=d;assert.equal(C.summarizeCloudClipping(records,s).matchingCount,1);}
assert.deepEqual([...records],original);assert.equal(C.cloudClipBounds(new Int16Array()),null);
assert.deepEqual(C.cloudClipBounds(new Int16Array([1,-2,3,-9])),{depth:{enabled:false,min:-9,max:-9},box:{enabled:false,min:[1,-2,3],max:[1,-2,3]}});
assert.deepEqual(C.summarizeCloudClipping(new Int16Array(),s),{matchingCount:0,clippedCount:0,submittedCount:0});
for(const invalid of [[],new Int16Array(3),new Int16Array(C.CLOUD_MAX*4+4)])assert.throws(()=>C.summarizeCloudClipping(invalid,defaults));
const max=new Int16Array(C.CLOUD_MAX*4);s.depth={enabled:true,min:0,max:0};assert.equal(C.summarizeCloudClipping(max,s).matchingCount,C.CLOUD_MAX);
// Exercise cached renderer methods without introducing a browser dependency into CTest.
const window={ChitonConsole:C};vm.runInNewContext(fs.readFileSync('release/files/console-cloud-view.js','utf8'),{window,Int16Array,Number,JSON,Error,performance});
const v=Object.create(window.ChitonConsole.CloudView.prototype),vec={set(){},fromArray(){}};
Object.assign(v,{clipSettings:C.defaultClipSettings(),clipUpdates:0,uniformUpdates:0,records,count:5,material:{uniforms:Object.fromEntries(['clipDepthEnabled','clipDepthRange','clipBoxEnabled','clipBoxMin','clipBoxMax'].map(k=>[k,{value:vec}]))},dirty(){this.dirties=(this.dirties||0)+1;}});
v.setClipSettings(s);assert.equal(v.clipUpdates,1);assert.equal(v.clipStats.matchingCount,1);v.setClipSettings(C.validateClipSettings(s));assert.equal(v.clipUpdates,1);
const saved=JSON.stringify(v.clipSettings);assert.throws(()=>v.setClipSettings({}));assert.equal(JSON.stringify(v.clipSettings),saved);assert.equal(v.dirties,1);
s.depth.min=-1;assert.equal(v.clipSettings.depth.min,0);
assert.notEqual(C.cloudClipContext({source:'demo',revision:1}),C.cloudClipContext({source:'demo',revision:2}));
assert.equal(C.cloudClipContext({source:'demo',revision:1,frameId:2}),C.cloudClipContext({source:'demo',revision:1,frameId:9}));
console.log('console_cloud_clipping: complete copied state, signed inclusive D/XYZ intersection, malformed inputs, exact counts, bounds seeding, max/empty and atomic cached setters passed');
