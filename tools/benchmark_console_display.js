#!/usr/bin/env node
/* Test-only loopback qualification. Never targets a device; GPU synchronization is test-only. */
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {fixture}=require('../tests/console_fixture'),{browserFor}=require('./console-browser-driver');
const C=require('../release/files/console-cloud');
(async()=>{
 const f=await fixture(),b=await browserFor(f.origin),out=path.resolve('_build/display-first-ui/validation'),results=[];fs.mkdirSync(out,{recursive:true});
 const off=C.defaultClipSettings(),depth=C.defaultClipSettings(),box=C.defaultClipSettings();depth.depth={enabled:true,min:900,max:1800};box.box={enabled:true,min:[-800,-900,-1000],max:[900,800,2500]};
 const clips={off,depth,box,intersection:{depth:depth.depth,box:box.box}};
 const quantile=(values,p)=>{const a=[...values].sort((a,b)=>a-b);return a.length?a[Math.min(a.length-1,Math.floor((a.length-1)*p))]:null;};
 const summary=values=>({n:values.length,median:quantile(values,.5),p95:quantile(values,.95)});
 try{
  f.source.mode=4;f.source.seek(6000);const original=f.source.cloudSnapshot.bind(f.source),full=original();let records=full.records;
  f.source.cloudSnapshot=()=>{const m=f.source.metadata();return {...full,frameId:m.frame_id,capturedAtMs:m.captured_at_ms,frameAgeMs:m.frame_age_ms,records,recordCount:records.length/4}};f.source.resume();
  await b.viewport(1280,720,1);await b.navigate(f.origin+'/console.html');await b.wait('ChitonConsoleApp.view.cloud?.count===307200');
  await b.evaluate(`(()=>{const a=ChitonConsoleApp,v=a.view.cloud;window.bench={latency:[],prepare:[],count:[],submission:[],readBodyDecode:[],uploads:0,uploadBytes:0,paints:0,resources:[]};
   const wrap=(obj,name,key)=>{const fn=obj[name];obj[name]=function(...args){const t=performance.now(),r=fn.apply(this,args);bench[key].push(performance.now()-t);return r;}};
   wrap(v,'update','prepare');wrap(ChitonConsole,'summarizeCloudClipping','count');wrap(v.renderer,'render','submission');
   const read=ChitonConsole.readCloudResponse;ChitonConsole.readCloudResponse=async function(...args){const t=performance.now();try{return await read.apply(this,args)}finally{bench.readBodyDecode.push(performance.now()-t)}};
   const gl=v.renderer.getContext();for(const name of ['bufferData','bufferSubData']){const fn=gl[name];gl[name]=function(...args){bench.uploads++;const arg=args[name==='bufferData'?1:2];bench.uploadBytes+=typeof arg==='number'?arg:(args[4]?args[4]*arg.BYTES_PER_ELEMENT:arg?.byteLength||0);return fn.apply(this,args)}};
   const render=v.render;v.render=function(...args){bench.paints++;return render.apply(this,args)};
   const accept=a.cloud.onValue;a.cloud.onValue=(value,info)=>{bench.latency.push(info.latency);return accept(value,info)};
   new PerformanceObserver(list=>{for(const e of list.getEntries())if(e.name.endsWith('/pointcloud/camera'))bench.resources.push({duration:e.duration,bytes:e.decodedBodySize})}).observe({type:'resource'});
   window.resetBench=()=>{for(const k of ['latency','prepare','count','submission','readBodyDecode','resources'])bench[k]=[];bench.uploads=bench.uploadBytes=bench.paints=0;};
   window.benchPose=v.nav.pose();v.host.style.width='800px';v.host.style.height='600px';v.resize();
  })()`);
  for(const count of [40000,307200]){
   records=full.records.slice(0,count*4);await b.sleep(600);
   for(const edl of [false,true])for(const [clipName,clip] of Object.entries(clips))for(const target of [2,10,20,30]){
    await b.evaluate(`(()=>{const a=ChitonConsoleApp;a.view.cloud.setClipSettings(${JSON.stringify(clip)});a.view.cloud.setEyeDomeLighting(${edl});document.getElementById('cloud-rate').value='${target}';document.getElementById('cloud-rate').dispatchEvent(new Event('change'));})()`);await b.sleep(150);
    const start=await b.evaluate('resetBench();({time:performance.now(),measurement:ChitonConsoleApp.measurement.requests,status:ChitonConsoleApp.status.requests})');await b.sleep(2200);
    const sample=await b.evaluate(`({elapsed:performance.now()-${start.time},...bench,counters:ChitonConsoleApp.view.cloud.counters(),distinct:ChitonConsoleApp.state.cloud.hz(performance.now()),measurement:ChitonConsoleApp.measurement.requests-${start.measurement},status:ChitonConsoleApp.status.requests-${start.status},heap:performance.memory?.usedJSHeapSize})`);
    assert.equal(sample.counters.points,count);assert.equal(f.maximum.cloud,1);assert(sample.measurement>0&&sample.status>0,'measurement/status starvation');
    results.push({count,edl,clipName,target,elapsed:sample.elapsed,distinctHz:sample.distinct,renderFPS:sample.counters.renderFPS,actualPayloadBytes:sample.resources.reduce((n,r)=>n+r.bytes,0),responseCount:sample.resources.length,latency:summary(sample.latency),readBodyAndDecode:summary(sample.readBodyDecode),preparation:summary(sample.prepare),clipCount:summary(sample.count),cpuSubmission:summary(sample.submission),uploads:sample.uploads,uploadBytes:sample.uploadBytes,measurementRequests:sample.measurement,statusRequests:sample.status,heap:sample.heap,counters:sample.counters});
   }
  }
  // Fixed cloud/pose/style/DPR, isolate enlarged-buffer cost from transport cadence.
  await b.evaluate('ChitonConsoleApp.stop();ChitonConsoleApp.view.cloud.setActive(true)');const layout=[];
  for(const [width,height] of [[800,600],[889,718]])for(const edl of [false,true]){
   const samples=await b.evaluate(`(()=>{const v=ChitonConsoleApp.view.cloud,gl=v.renderer.getContext(),p=new Uint8Array(4);v.host.style.width='${width}px';v.host.style.height='${height}px';v.resize();v.setEyeDomeLighting(${edl});v.setClipSettings(ChitonConsole.defaultClipSettings());const a=[];for(let i=0;i<12;i++){const t=performance.now();v.render();gl.finish();gl.readPixels(0,0,1,1,gl.RGBA,gl.UNSIGNED_BYTE,p);if(i>=4)a.push(performance.now()-t)}return {a,counters:v.counters()}})()`);layout.push({width,height,edl,completedDraw:summary(samples.a),samples:samples.a,counters:samples.counters});
  }
  // Delayed and stalled transport: bounded degradation and independent progress, no catch-up.
  await b.evaluate('ChitonConsoleApp.start()');const constrained=[];
  for(const mode of ['delay-header','delay-body','hang']){
   f.control.cloud=mode;const start=await b.evaluate('({time:performance.now(),cloud:ChitonConsoleApp.cloud.requests,measurement:ChitonConsoleApp.measurement.requests,status:ChitonConsoleApp.status.requests})');await b.sleep(3200);const end=await b.evaluate('({time:performance.now(),cloud:ChitonConsoleApp.cloud.requests,measurement:ChitonConsoleApp.measurement.requests,status:ChitonConsoleApp.status.requests,error:ChitonConsoleApp.state.cloud.error})');assert(end.measurement>start.measurement&&end.status>start.status);assert.equal(f.maximum.cloud,1);constrained.push({mode,start,end});
  }
  f.control.cloud='ok';await b.sleep(1500);const sustainedStart=await b.evaluate('({time:performance.now(),cloud:ChitonConsoleApp.cloud.requests,measurement:ChitonConsoleApp.measurement.requests,status:ChitonConsoleApp.status.requests})');await b.sleep(10000);const sustainedEnd=await b.evaluate('({time:performance.now(),cloud:ChitonConsoleApp.cloud.requests,measurement:ChitonConsoleApp.measurement.requests,status:ChitonConsoleApp.status.requests,counters:ChitonConsoleApp.view.cloud.counters()})');
  assert.equal(f.maximum.cloud,1);assert.equal(f.source.mutations.length,0);assert.deepEqual(b.errors,[]);
  fs.writeFileSync(path.join(out,'throughput.json'),JSON.stringify({browser:await b.command('Browser.getVersion',{},null),results,layout,constrained,sustained:{start:sustainedStart,end:sustainedEnd},maximum:f.maximum,limitations:'Loopback fixture, Chrome software WebGL, not physical GPU/network/device throughput. HTTP body+decode timing combines transfer and parser, not isolated decode. Resource byte counts cover completed responses, not socket overhead. CPU submission separate from test-only synchronized completed draws. Heap/counters are not GPU memory. Fixed synthetic frame content; distinct source IDs follow source clock.'},null,2));console.log('benchmark_console_display: '+results.length+' fixed-buffer rate/style/clip cases, enlarged-buffer draws, constrained/stalled transfers and sustained run; one flight, measurement/status progress');
 }finally{await b.close();await f.close()}
})().catch(e=>{console.error(e);process.exitCode=1});
