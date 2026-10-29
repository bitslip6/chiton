'use strict';
const assert=require('node:assert/strict'),C=require('../release/files/console-cloud'),{defaults,encodeCleanup}=require('./cleanup_fixture');
const cfg={...defaults(),enabled:1,algorithm:2},records=new Int16Array([-32768,0,32767,1500,50,-1,0,1000,1,2,3,2000]);
const snapshot={records,frameId:8,revision:3,capturedAtMs:1234,units:'mm'};
for(const algorithm of [1,2,3])for(const byteOrder of ['little','big']){
 cfg.algorithm=algorithm;
 const b=encodeCleanup(snapshot,cfg,7,{flags:[8,1,4],byteOrder});
 const m=C.decodeCleanup(b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength));
 assert.deepEqual(m.records,records);assert.deepEqual([...m.cleanup.flags],[8,1,4]);assert.equal(m.cleanup.generation,7);assert.equal(m.cleanup.config.algorithm,algorithm);
 assert.equal(m.cleanup.mixed,1);assert.equal(m.frameId,8);assert.equal(m.units,'mm');
 for(const mutate of [b=>b[0]=0,b=>b.writeUInt32LE(4,12),b=>b[128+records.length*2]=16,b=>b.writeUInt32LE(99,92),b=>b.writeUInt32LE(4,56),b=>b[71]=0]){
  const bad=Buffer.from(b);mutate(bad);assert.throws(()=>C.decodeCleanup(bad.buffer.slice(bad.byteOffset,bad.byteOffset+bad.byteLength)));
 }
 assert.throws(()=>C.decodeCleanup(b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength-1)));
}
for(const change of [{boundary_gap:80},{center_margin:40},{surface_support:0},{center_support:255},{evidence_min:65535,evidence_max:0},{ramp_residual:65535},{anchor_tolerance:0}]){
 const expected={...cfg,...change};assert.deepEqual(C.validateCleanupConfig(expected),expected);
 const b=encodeCleanup(snapshot,expected,8,{flags:[8,1,4]});
 assert.deepEqual(C.decodeCleanup(b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength)).cleanup.config,expected);
}
for(const change of [{algorithm:4},{boundary_gap:65536},{boundary_gap:-1},{boundary_gap:1.5},{surface_support:256},{center_support:256},{algorithm:0},{evidence_max:65536}])assert.throws(()=>C.validateCleanupConfig({...cfg,...change}));
assert.deepEqual(C.validateCleanupConfig(cfg),cfg);
const settings={generation:1,config:cfg,available:true,status:2,frame_generation:1};
assert.throws(()=>C.validateCleanupSettings(settings),'Old server must not imply algorithm 3 support');
assert.deepEqual(C.validateCleanupSettings({...settings,constraints:{algorithm:[1,2,3]}}).algorithms,[1,2,3]);
assert.deepEqual(C.validateCleanupSettings({...settings,config:{...cfg,algorithm:1}}).algorithms,[1,2]);
const invalidEarly=encodeCleanup(snapshot,cfg,7,{flags:[1,1,4]});invalidEarly.writeUInt32LE(1,88);invalidEarly.writeUInt32LE(1,112);
assert.throws(()=>C.decodeCleanup(invalidEarly.buffer.slice(invalidEarly.byteOffset,invalidEarly.byteOffset+invalidEarly.byteLength)));
assert.match(C.cleanupDescription(cfg),/radius 6/);
(async()=>{
 const {LiveSource}=require('../release/files/console-live'),live=new LiveSource(),original=global.fetch;live.mode=4;
 try{
  let posts=0;global.fetch=async()=>{posts++;throw new Error('unexpected request');};
  await assert.rejects(()=>live.applyCleanup({config:cfg,generation:1}),/not supported/);assert.equal(posts,0);
  live.cleanupAvailable=true;
  global.fetch=async route=>{assert.equal(route,'/api/cleanup/v1/cloud');live.cleanupAvailable=false;const b=encodeCleanup(snapshot,cfg,7,{flags:[8,1,4]});return new Response(b,{headers:{'Content-Type':'application/octet-stream','Content-Length':String(b.length)}});};
  assert.equal((await live.readCloud({})).cleanup.generation,7);
  live.cleanupAvailable=false;
  global.fetch=async route=>{assert.equal(route,'/pointcloud/camera');live.cleanupAvailable=true;return new Response(Buffer.from(records.buffer),{headers:{'Content-Type':'application/octet-stream','Content-Length':String(records.byteLength),
   'X-Chiton-Layout':'int16_xyzd','X-Chiton-Color-Mode':'4','X-Chiton-Frame-Type':'depth','X-Chiton-Units':'mm','X-Chiton-Source-Units':'mm','X-Chiton-Units-Scope':'calibration_aoi','X-Chiton-Invalid-Sample-Policy':'all_uint16_included',
   'X-Chiton-Byte-Order':'little','X-Chiton-Width':'3','X-Chiton-Height':'1','X-Chiton-Frame-Id':'8','X-Chiton-Configuration-Revision':'3'}});};
  assert.deepEqual((await live.readCloud({})).records,records);
 }finally{global.fetch=original;}
 console.log('console_cleanup: signed/endian same-frame geometry/flags, identity, bounded validation, configuration predicates and in-flight capability/decoder races passed');
})().catch(e=>{console.error(e);process.exitCode=1;});
