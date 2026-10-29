/* Real WebGL and loopback live-source camera playback, never a device. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
async function gpuMotion(){
 const C=ChitonConsole,T=THREE,host=document.createElement('div');host.style.cssText='position:fixed;width:256px;height:256px';document.body.append(host);
 const v=new C.CloudView(host);if(v.raf!==null)cancelAnimationFrame(v.raf);v.raf=null;v.dirty=()=>{};v.axes.visible=false;
 const check=(ok,msg)=>{if(!ok)throw new Error(msg);},near=(a,b)=>Math.abs(a-b)<1e-8;
 const records=new Int16Array([-100,0,0,1000,0,0,0,1000,100,0,0,1000,0,100,0,1000,0,-100,0,1000]);
 const snapshot={records,recordCount:5,source:'test',revision:1,frameType:'depth',units:'mm',unitsScope:'calibration_aoi',frameId:1};
 v.update(snapshot,{min:500,max:1500});v.setPointStyle({shape:'circle',diameterCssPx:10,sizing:'screen'});
 const timeline=new C.CameraTimeline(),a={target:[0,0,0],radius:1000,theta:0,phi:Math.PI/2},b={target:[20,10,0],radius:1500,theta:Math.PI/2,phi:1.2};
 timeline.add(0,a,'orthographic');timeline.add(2,b,'orthographic');v.setCameraTimeline(timeline);v.setEyeDomeLighting(true);
 const output=new T.WebGLRenderTarget(256,256),gl=v.renderer.getContext(),clock=performance.now();
 v.playCameraMotion(clock);v.renderer.setRenderTarget(output);v.render(clock);
 const uploads=v.uploads,reference=v.referenceDistance,sourceReference=v.referenceDepth,version=v.attributeBuffer.version;
 const poses=[];
 for(const seconds of [.25,.5,1,1.5,2]){
  v.render(clock+seconds*1000);const expected=timeline.sample(seconds),actual=v.nav.pose();
  check(near(actual.radius,expected.radius)&&near(actual.theta,expected.theta)&&near(actual.phi,expected.phi)&&actual.target.every((x,k)=>near(x,expected.target[k])),'render uses eased camera sample');
  check(v.uploads===uploads&&v.attributeBuffer.version===version,'camera-only paint uploaded geometry');
  check(v.referenceDistance===reference&&v.referenceDepth===sourceReference,'camera playback recaptured sizing references');
  const pixels=new Uint8Array(256*256*4);v.renderer.readRenderTargetPixels(output,0,0,256,256,pixels);
  check(pixels.some((x,i)=>i%4===1&&x>80),'actual moving point pixels');poses.push(actual);
 }
 check(!timeline.playing&&timeline.time===2,'finite playback ends');
 const original=[...records];v.playCameraMotion(clock+3000);v.update({...snapshot,frameId:2},null);v.render(clock+3500);
 check(timeline.playing&&near(timeline.time,.5),'new live frame did not pause/refit camera');check(JSON.stringify([...records])===JSON.stringify(original),'source records changed');
 v.setActive(false);const paused=timeline.time;v.setActive(true);check(!timeline.playing&&timeline.time===paused,'inactive view auto-resumed');
 v.playCameraMotion(clock+4000);v.canvas.dispatchEvent(new WheelEvent('wheel',{deltaY:1,cancelable:true}));check(!timeline.playing,'manual wheel must pause');
 check(!v.edlError&&gl.getError()===gl.NO_ERROR,'moving orthographic EDL failed');
 const webgl=gl.getParameter(gl.VERSION);output.dispose();v.dispose();host.remove();return {poses,uploads,reference,sourceReference,webgl,paused};
}
(async()=>{
 const f=await fixture();f.source.mode=4;f.source.scenario='cloud-single';
 // Small, explicitly synthetic changing XYZD frame, not just changing frame IDs.
 const originalCloud=f.source.cloudSnapshot.bind(f.source);
 f.source.cloudSnapshot=()=>{const s=originalCloud(),phase=f.source.metadata().frame_id%100;return {...s,records:new Int16Array([phase,0,1000,1000+phase]),recordCount:1};};
 const b=await browserFor(f.origin),out=path.resolve(process.env.CHITON_CAMERA_EVIDENCE||'_build/camera-timeline/validation');fs.mkdirSync(out,{recursive:true});
 const app='ChitonConsoleApp',evalJS=(s,userGesture=false)=>b.evaluate(s,userGesture),click=id=>evalJS(`document.getElementById('${id}').click()`),keys=()=>evalJS(`${app}.state.cameraTimeline.keyframes`);
 const add=async time=>{await evalJS(`document.getElementById('camera-key-time').value=${time}`);await click('camera-key-add');};
 const pose=()=>evalJS(`${app}.view.cloud.nav.pose()`);
 try{
  await b.viewport(1280,800);await b.navigate(f.origin+'/console.html?source=live');await b.wait(`${app}.view.cloud?.count===1 && !document.getElementById('camera-key-add').disabled`);
  await evalJS(`document.getElementById('cloud-rate').value='20';document.getElementById('cloud-rate').dispatchEvent(new Event('change'));`);
  await add(0);const initial=await pose();
  await evalJS(`${app}.view.cloud.nav.orbit(1,.2);${app}.view.cloud.nav.pan(5,-3);${app}.view.cloud.nav.zoom(.3);${app}.view.cloud.dirty()`);await add(4);
  assert.equal((await keys()).length,2);assert.equal(await evalJS(`${app}.state.cameraTimeline.duration`),4);
  // Atomic retime rejection, then a valid absolute-second edit.
  await evalJS(`(()=>{const e=document.querySelectorAll('.camera-keyframe input')[1];e.value='0';e.dispatchEvent(new Event('change'));})()`);
  assert.equal(await evalJS(`document.getElementById('camera-motion-error').hidden`),false);assert.equal((await keys())[1].time,4);
  await evalJS(`(()=>{const e=document.querySelectorAll('.camera-keyframe input')[1];e.value='5';e.dispatchEvent(new Event('change'));})()`);
  assert.equal((await keys())[1].time,5);assert.equal(await evalJS(`document.getElementById('camera-motion-error').hidden`),true);
  await evalJS(`${app}.view.cloud.nav.orbit(.3,.1)`);const updated=await pose();
  await evalJS(`document.querySelectorAll('.camera-keyframe')[1].querySelectorAll('button')[1].click()`);assert.deepEqual((await keys())[1].pose,updated);
  await click('camera-stop');assert.deepEqual(await pose(),initial);
  const before=await evalJS(`({frame:${app}.state.cloud.latest.frameId,uploads:${app}.view.cloud.uploads,reads:${app}.cloud.requests,measurement:${app}.measurement.requests,records:[...${app}.state.cloud.latest.records]})`);
  await click('camera-play');await b.wait(`${app}.state.cameraTimeline.time>.7 && ${app}.state.cloud.latest.frameId>${before.frame}`);
  const live=await evalJS(`({frame:${app}.state.cloud.latest.frameId,uploads:${app}.view.cloud.uploads,reads:${app}.cloud.requests,measurement:${app}.measurement.requests,time:${app}.state.cameraTimeline.time,playing:${app}.state.cameraTimeline.playing,records:[...${app}.state.cloud.latest.records]})`);
  assert(live.playing&&live.uploads>before.uploads&&live.reads>before.reads&&live.measurement>before.measurement);assert.notDeepEqual(await pose(),initial);assert.notDeepEqual(live.records,before.records,'live XYZD values changed during motion');assert.equal(f.maximum.cloud,1);
  await click('camera-pause');const held=await evalJS(`${app}.state.cameraTimeline.time`);await b.sleep(250);assert.equal(await evalJS(`${app}.state.cameraTimeline.time`),held);
  await click('camera-play');await b.wait(`${app}.state.cameraTimeline.time>${held}+.1`);
  await evalJS(`${app}.view.cloud.canvas.dispatchEvent(new WheelEvent('wheel',{deltaY:20,cancelable:true}))`);assert.equal(await evalJS(`${app}.state.cameraTimeline.playing`),false);
  // Scrub and explicit stop apply the recorded camera without touching source settings.
  await evalJS(`document.getElementById('camera-seek').value='2.5';document.getElementById('camera-seek').dispatchEvent(new Event('input'))`);
  assert.equal(await evalJS(`${app}.state.cameraTimeline.time`),2.5);await click('camera-stop');assert.deepEqual(await pose(),initial);
  await evalJS(`${app}.view.cloud.applyCameraPose(${JSON.stringify(initial)})`);await add(8);
  await evalJS(`document.getElementById('camera-loop').click();${app}.view.cloud.seekCameraMotion(7.9)`);await click('camera-play');await b.wait(`${app}.state.cameraTimeline.time<1 && ${app}.state.cameraTimeline.playing`);
  assert.equal((await keys()).length,3);
  // Hidden/page pause freezes the clock and never auto-resumes it.
  await evalJS(`Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'))`);
  const hidden=await evalJS(`${app}.state.cameraTimeline.time`);assert.equal(await evalJS(`${app}.view.cloud.raf`),null);
  await evalJS(`Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'))`);await b.wait(`${app}.cloud.running`);await b.sleep(150);
  assert.equal(await evalJS(`${app}.state.cameraTimeline.playing`),false);assert.equal(await evalJS(`${app}.state.cameraTimeline.time`),hidden);
  await click('camera-play');await click('pause-viewer');const pagePause=await evalJS(`${app}.state.cameraTimeline.time`);await b.sleep(150);await click('pause-viewer');await b.wait(`${app}.cloud.running`);
  assert.equal(await evalJS(`${app}.state.cameraTimeline.time`),pagePause);assert.equal(await evalJS(`${app}.state.cameraTimeline.playing`),false);
  // Explicit context loss retains keys but stops playback; coordinate changes clear them.
  const saved=await keys();await evalJS(`${app}.view.cloud.renderer.forceContextLoss()`);await b.wait(`${app}.state.cloudUnavailable!==null`);await click('cloud-retry');await b.wait(`${app}.view.cloud?.count===1`);
  assert.deepEqual(await keys(),saved);assert.equal(await evalJS(`${app}.state.cameraTimeline.playing`),false);
  f.source.revision++;await b.wait(`${app}.state.cameraTimeline.count===0 && document.getElementById('camera-motion-status').textContent.includes('coordinate context')`);
  // New frozen live page: camera-only animation must make zero attribute uploads.
  f.source.seek(1000);await b.navigate(f.origin+'/console.html?source=live');await b.wait(`${app}.view.cloud?.count===1 && ${app}.state.cloud.latest?.frameId===31`);
  await add(0);await evalJS(`${app}.view.cloud.nav.orbit(1,.2);${app}.view.cloud.nav.zoom(.3)`);await add(1.5);await click('camera-stop');
  const frozen=await evalJS(`(()=>{const a=${app},v=a.view.cloud;v.render();const gl=v.renderer.getContext();window.cameraUploads=0;for(const name of ['bufferData','bufferSubData']){const old=gl[name].bind(gl);gl[name]=(...args)=>{if(args[0]===gl.ARRAY_BUFFER)cameraUploads++;return old(...args);};}return {uploads:v.uploads,records:[...v.records.slice(0,4)],measurement:a.state.latest.value,history:a.state.history.items.length};})()`);
  await click('camera-play');await b.wait(`${app}.state.cameraTimeline.time>.5`);assert.equal(await evalJS('cameraUploads'),0);assert.equal(await evalJS(`${app}.view.cloud.uploads`),frozen.uploads);
  assert.deepEqual(await evalJS(`[...${app}.view.cloud.records.slice(0,4)]`),frozen.records);assert.equal(await evalJS(`${app}.state.latest.value`),frozen.measurement);assert.equal(await evalJS(`${app}.state.history.items.length`),frozen.history);
  await b.wait(`!${app}.state.cameraTimeline.playing`);assert.equal(await evalJS(`${app}.state.cameraTimeline.time`),1.5);await b.sleep(100);assert.equal(await evalJS(`${app}.view.cloud.raf`),null);
  // Controls remain local, accessible in the same fullscreen/narrow panel.
  await evalJS(`document.getElementById('display-controls').hidden=false;document.getElementById('camera-motion').open=true;for(const e of document.querySelectorAll('#display-controls details'))if(e.id!=='camera-motion')e.open=false;`);
  for(const [w,h] of [[1280,800],[390,844],[320,844]]){await b.viewport(w,h);assert.equal(await evalJS('document.documentElement.scrollWidth<=innerWidth'),true);}
  await b.screenshot(path.join(out,'camera-timeline-mobile.png'));
  await evalJS(`document.getElementById('fullscreen').click()`,true);await b.wait('document.fullscreenElement?.id==="viewport"');assert.equal(await evalJS('document.fullscreenElement.contains(document.getElementById("camera-motion"))'),true);
  await evalJS('document.exitFullscreen()',true);await click('camera-clear');assert.equal((await keys()).length,0);
  const reports=[];
  for(const webgl1 of [false,true]){
   let injection;if(webgl1)injection=await b.command('Page.addScriptToEvaluateOnNewDocument',{source:`const old=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(t,...a){return t==='webgl2'?null:old.call(this,t,...a);};`});
   await b.navigate(f.origin+'/console.html?source=demo&scenario=cloud-single');await b.evaluate('ChitonConsoleApp.prepareCapture({cloudMode:true})');reports.push(await b.evaluate(`(${gpuMotion.toString()})()`));if(injection)await b.command('Page.removeScriptToEvaluateOnNewDocument',{identifier:injection.identifier});
  }
  assert.equal(f.source.mutations.length,0);assert.deepEqual(b.errors,[]);
  fs.writeFileSync(path.join(out,'camera-browser.json'),JSON.stringify({passed:true,live,frozen,reports,errors:b.errors,mutations:f.source.mutations},null,2));
  console.log('console_camera_timeline_browser: actual WebGL1/2 eased poses/EDL, live point updates during playback, clock/lifecycle/manual takeover, keyframe edit/scrub/loop/clear, no camera-only uploads/device writes, context safety and narrow/fullscreen controls passed');
 }finally{await b.close();await f.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
