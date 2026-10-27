/* Local renderer controls only: no device access or mutations. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
(async()=>{
 const mock=await fixture(),b=await browserFor(mock.origin),out=path.resolve(process.env.CHITON_DEPTH_STYLE_EVIDENCE||'_build/renderer-depth-style/validation');fs.mkdirSync(out,{recursive:true});
 const prefs=()=>b.evaluate(`(()=>{const a=ChitonConsoleApp,v=a.view.cloud;return {reverse:a.state.reverseDepthColors,strength:a.state.edlStrength,sizing:a.state.pointStyle.sizing,renderer:v?{reverse:v.reverseDepthColors,strength:v.edlStrength,sizing:v.pointStyle.sizing}:null};})()`);
 const choose=()=>b.evaluate(`(()=>{const e=id=>document.getElementById(id);e('point-reverse-colors').click();e('point-sizing').value='depth';e('point-sizing').dispatchEvent(new Event('change'));e('cloud-edl-strength').value='8';e('cloud-edl-strength').dispatchEvent(new Event('input'));})()`);
 const expected={reverse:true,strength:8,sizing:'depth',renderer:{reverse:true,strength:8,sizing:'depth'}};
 try{
  await b.viewport(1280,720);await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');
  await b.evaluate('ChitonConsoleApp.prepareCapture({cloudMode:true,eyeDomeLighting:true})');
  assert.deepEqual(await prefs(),{reverse:false,strength:4,sizing:'screen',renderer:{reverse:false,strength:4,sizing:'screen'}});
  const baseline=await b.evaluate(`(()=>{const a=ChitonConsoleApp,v=a.view.cloud,g=v.renderer.getContext();window.depthUploads=0;for(const k of ['bufferData','bufferSubData']){const old=g[k].bind(g);g[k]=(...args)=>{if(args[0]===g.ARRAY_BUFFER)depthUploads++;return old(...args);};}return {pose:v.nav.pose(),records:[...v.records.slice(0,40)],reference:v.referenceDepth,measurements:JSON.stringify({latest:a.state.latest,history:a.state.history.items,kalman:a.state.kalman}),storage:JSON.stringify(localStorage)};})()`);
  await choose();await b.wait('ChitonConsoleApp.view.cloud.edl.material.uniforms.strength.value===8');
  assert.deepEqual(await prefs(),expected);
  assert.equal(await b.evaluate('depthUploads'),0);
  assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.material.uniforms.reverseDepthColors.value'),true);
  assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.material.uniforms.depthSizing.value'),true);
  assert.match(await b.evaluate('document.getElementById("cloud-legend").textContent'),/red near.*blue far/);
  assert.equal(await b.evaluate('document.getElementById("cloud-legend").classList.contains("depth-color-reversed")'),true);
  const after=await b.evaluate(`(()=>{const a=ChitonConsoleApp,v=a.view.cloud;return {pose:v.nav.pose(),records:[...v.records.slice(0,40)],reference:v.referenceDepth,measurements:JSON.stringify({latest:a.state.latest,history:a.state.history.items,kalman:a.state.kalman}),storage:JSON.stringify(localStorage)};})()`);
  assert.deepEqual(after,baseline);
  const footer=await b.evaluate(`(async()=>{const lines=[],old=CanvasRenderingContext2D.prototype.fillText;CanvasRenderingContext2D.prototype.fillText=function(t,...a){lines.push(String(t));return old.call(this,t,...a);};try{await ChitonConsoleApp.view.cloud.png(ChitonConsoleApp.state.cloud.latest,{source:'demo',overlays:false});}finally{CanvasRenderingContext2D.prototype.fillText=old;}return lines.join(' ');})()`);
  assert.match(footer,/red near.*blue far/);assert.match(footer,/strength 8/);assert.match(footer,/sensor D \(near larger\)/);
  await b.evaluate(`document.getElementById('display-controls').hidden=false;document.getElementById('point-appearance').open=true;`);
  for(const [w,h] of [[1280,720],[390,844],[320,844]]){
   await b.viewport(w,h,1);assert.equal(await b.evaluate('document.documentElement.scrollWidth<=innerWidth'),true);
  }
  await b.screenshot(path.join(out,'mobile-reversed-depth-edl.png'));
  // Renderer recreation retains all page-local preferences.
  await b.evaluate(`(async()=>{const a=ChitonConsoleApp;a.source.mode=1;a.applyStatus(await a.source.readStatus());a.source.mode=4;a.applyStatus(await a.source.readStatus());await a.prepareCapture({cloudMode:true});})()`);
  assert.deepEqual(await prefs(),expected);
  const capture=await b.evaluate('ChitonConsoleApp.prepareCapture({cloudMode:true,reverseDepthColors:true,edlStrength:6,pointStyle:{shape:"circle",diameterCssPx:3,sizing:"depth"}})');
  assert.equal(capture.cloud.renderer.reverseDepthColors,true);assert.equal(capture.cloud.renderer.edlStrength,6);
  for(const args of [{edlStrength:9},{reverseDepthColors:1}])assert.equal(await b.evaluate(`(async()=>{try{await ChitonConsoleApp.prepareCapture(${JSON.stringify(args)});return false;}catch(_){return true;}})()`),true);
  await b.evaluate('document.getElementById("reset-appearance").click()');
  assert.deepEqual(await prefs(),{reverse:false,strength:4,sizing:'screen',renderer:{reverse:false,strength:4,sizing:'screen'}});
  assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.eyeDomeLighting'),true,'appearance reset leaves lighting enable unchanged');
  // Frozen live page: paused local changes cannot paint, fetch, or alter device settings.
  mock.source.mode=4;mock.source.seek(1000);await b.navigate(mock.origin+'/console.html?source=live');await b.wait('ChitonConsoleApp.view.cloud?.count>0');
  await b.evaluate('document.getElementById("pause-viewer").click()');
  const paused=await b.evaluate('({paint:ChitonConsoleApp.view.cloud.lastPaint,uploads:ChitonConsoleApp.view.cloud.uploads})'),requests=mock.requests.length;
  await choose();assert.deepEqual(await prefs(),expected);
  assert.deepEqual(await b.evaluate('({paint:ChitonConsoleApp.view.cloud.lastPaint,uploads:ChitonConsoleApp.view.cloud.uploads})'),paused);
  assert.equal(mock.requests.length,requests);assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.raf'),null);
  assert.equal(mock.source.mutations.length,0);assert.deepEqual(b.errors,[]);
  fs.writeFileSync(path.join(out,'depth-style-ui.json'),JSON.stringify({passed:true,capture,footer,errors:b.errors,mutations:mock.source.mutations},null,2));
  console.log('console_depth_style_browser: local reversal/legend, sensor-depth sizing, EDL strength, zero attribute uploads/device writes, unchanged pose/measurements, recreation/reset/capture/PNG and paused live controls passed');
 }finally{await b.close();await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
