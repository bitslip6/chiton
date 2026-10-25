/* Real input, single-tree lifecycle, source-default safety and 30 Hz wiring; loopback only. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
(async()=>{
 const f=await fixture(),b=await browserFor(f.origin),out=path.resolve('_build/display-first-ui/validation');fs.mkdirSync(out,{recursive:true});
 const $=id=>`document.getElementById(${JSON.stringify(id)})`;
 const click=async id=>{await b.evaluate(`${$(id)}.scrollIntoView({block:'nearest'})`);const r=await b.evaluate(`(()=>{const r=${$(id)}.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})()`);for(const type of ['mousePressed','mouseReleased'])await b.command('Input.dispatchMouseEvent',{type,...r,button:'left',clickCount:1});};
 const key=async(key,code,n)=>{for(const type of ['keyDown','keyUp'])await b.command('Input.dispatchKeyEvent',{type,key,code,windowsVirtualKeyCode:n});};
 const rate=value=>b.evaluate(`(()=>{const e=${$('cloud-rate')};e.value=${JSON.stringify(String(value))};e.dispatchEvent(new Event('change'));})()`);
 const report={};
 try{
  await b.command('Page.addScriptToEvaluateOnNewDocument',{source:`window.storageWrites=0;const set=Storage.prototype.setItem;Storage.prototype.setItem=function(...a){storageWrites++;return set.apply(this,a)};window.demoConstructions=0;document.addEventListener('DOMContentLoaded',()=>{});`});
  // Default/empty/explicit live all follow acknowledged mode with no startup writes.
  f.source.mode=4;f.source.scenario='cloud-single';
  for(const query of ['', '?source=', '?source=live']){
   await b.viewport(1280,720);await b.navigate(f.origin+'/console.html'+query);await b.wait('ChitonConsoleApp.state.cloud.latest!=null');
   assert.equal(await b.evaluate('ChitonConsoleApp.state.sourceName'),'live');assert.equal(await b.evaluate('storageWrites'),0);assert.equal(f.source.mutations.length,0);
   assert.equal(await b.evaluate(`(async()=>{const a=ChitonConsoleApp,before=JSON.stringify({paused:a.state.paused,hz:a.state.cloudHz,requests:a.cloud.requests});try{await a.prepareCapture();return false;}catch(e){return /demo/.test(e.message)&&before===JSON.stringify({paused:a.state.paused,hz:a.state.cloudHz,requests:a.cloud.requests})}})()`),true);
  }
  await rate(30);await b.sleep(2200);report.fast=await b.evaluate('({target:ChitonConsoleApp.cloud.hz,distinct:ChitonConsoleApp.state.cloud.hz(performance.now()),requests:ChitonConsoleApp.cloud.requests,fps:ChitonConsoleApp.view.cloud.counters().renderFPS})');assert.equal(report.fast.target,30);assert(report.fast.distinct>20,JSON.stringify(report.fast));assert.equal(f.maximum.cloud,1);
  f.control.cloud='delay-header';const requests=f.counts['/pointcloud/camera'],measurements=await b.evaluate('ChitonConsoleApp.measurement.requests');await b.sleep(1500);assert(f.counts['/pointcloud/camera']-requests<=7);assert((await b.evaluate('ChitonConsoleApp.measurement.requests'))>measurements);assert.equal(f.maximum.cloud,1);f.control.cloud='ok';
  await click('pause-viewer');await b.sleep(100);const reads=f.requests.length;
  await rate(20);assert.equal(await b.evaluate('ChitonConsoleApp.state.cloudHz'),20);assert.equal(await b.evaluate('ChitonConsoleApp.cloud.running'),false);
  for(const invalid of ['','NaN','Infinity','0','-1','31','3e1','30junk']){
   await b.evaluate(`(()=>{const e=${$('cloud-rate')},o=new Option(${JSON.stringify(invalid)},${JSON.stringify(invalid)});e.add(o);e.value=o.value;e.dispatchEvent(new Event('change'));o.remove()})()`);
   assert.equal(await b.evaluate('ChitonConsoleApp.state.cloudHz'),20);assert.equal(await b.evaluate('ChitonConsoleApp.cloud.hz'),20);
  }
  await b.sleep(150);assert.equal(f.requests.length,reads);await click('pause-viewer');await b.wait('ChitonConsoleApp.cloud.running');assert.equal(await b.evaluate('ChitonConsoleApp.cloud.hz'),20);
  // Demo remains explicit and the closed panel contains no focusable visible descendants.
  await b.navigate(f.origin+'/console.html?source=demo&scenario=cloud');await b.evaluate('ChitonConsoleApp.prepareCapture()');await b.sleep(100);const demoReads=f.requests.length;
  assert.equal(await b.evaluate(`${$('display-controls')}.hidden`),true);await b.evaluate(`${$('cloud-rate')}.focus()`);assert.notEqual(await b.evaluate('document.activeElement.id'),'cloud-rate');
  await click('toggle-display-controls');assert.equal(await b.evaluate(`${$('toggle-display-controls')}.getAttribute('aria-expanded')`),'true');
  await b.evaluate(`${$('close-display-controls')}.focus()`);await key('Escape','Escape',27);assert.equal(await b.evaluate('document.activeElement.id'),'toggle-display-controls');assert.equal(await b.evaluate(`${$('display-controls')}.hidden`),true);
  // Touch opens the same panel; scrolling/input keys cannot reach canvas handlers.
  await b.viewport(390,844,2);const touch=await b.evaluate(`(()=>{const r=${$('toggle-display-controls')}.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})()`);
  await b.command('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{...touch,id:1}]});await b.command('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await b.wait(`!${$('display-controls')}.hidden`);
  await b.evaluate(`${$('point-appearance')}.open=true;${$('clip-details')}.open=true`);
  const pose=await b.evaluate('ChitonConsoleApp.view.cloud.nav.pose()');await b.evaluate(`${$('point-diameter')}.focus()`);await key('ArrowRight','ArrowRight',39);assert.deepEqual(await b.evaluate('ChitonConsoleApp.view.cloud.nav.pose()'),pose);
  await b.evaluate(`${$('clip-d-min')}.value='';${$('clip-d-min')}.dispatchEvent(new Event('input',{bubbles:true}));${$('display-controls')}.scrollTop=200`);
  const saved=await b.evaluate(`({draft:ChitonConsoleApp.state.clipDraft,scroll:${$('display-controls')}.scrollTop,error:${$('clip-error')}.textContent})`);
  await b.evaluate(`${$('close-display-controls')}.click();${$('toggle-display-controls')}.click()`);
  assert.deepEqual(await b.evaluate(`({draft:ChitonConsoleApp.state.clipDraft,scroll:${$('display-controls')}.scrollTop,error:${$('clip-error')}.textContent})`),saved);
  const rect=await b.evaluate(`(()=>{const r=${$('display-controls')}.getBoundingClientRect();return {x:r.x+40,y:r.y+100}})()`);await b.command('Input.dispatchMouseEvent',{type:'mouseWheel',...rect,deltaX:0,deltaY:80});assert.deepEqual(await b.evaluate('ChitonConsoleApp.view.cloud.nav.pose()'),pose);
  // 100 toggles: same renderer/DOM/listeners, no new uploads, paints, network or storage.
  await b.evaluate(`ChitonConsoleApp.stop();window.panelProbe={renderer:ChitonConsoleApp.view.cloud,uploads:ChitonConsoleApp.view.cloud.uploads,last:ChitonConsoleApp.view.cloud.lastPaint,adds:0};window.panelAdd=EventTarget.prototype.addEventListener;EventTarget.prototype.addEventListener=function(...a){panelProbe.adds++;return panelAdd.apply(this,a)}`);
  report.cycles=await b.evaluate(`(()=>{const a=ChitonConsoleApp,panel=${$('display-controls')},before=[...panel.querySelectorAll('[id]')];for(let i=0;i<100;i++){${$('close-display-controls')}.click();${$('toggle-display-controls')}.click();}EventTarget.prototype.addEventListener=panelAdd;return {same:panelProbe.renderer===a.view.cloud&&before.every(e=>document.getElementById(e.id)===e),adds:panelProbe.adds,uploads:a.view.cloud.uploads-panelProbe.uploads,paints:a.view.cloud.lastPaint-panelProbe.last,raf:a.view.cloud.raf,storage:storageWrites,ids:[...document.querySelectorAll('[id]')].map(e=>e.id)}})()`);
  assert.equal(report.cycles.same,true);assert.equal(report.cycles.adds,0);assert.equal(report.cycles.uploads,0);assert.equal(report.cycles.paints,0);assert.equal(report.cycles.raf,null);assert.equal(report.cycles.storage,0);assert.equal(new Set(report.cycles.ids).size,report.cycles.ids.length);
  // Modal handoff and focus restoration normal/fullscreen; one overlay root throughout.
  for(const fullscreen of [false,true]){
   if(fullscreen){await b.evaluate(`${$('fullscreen')}.click()`,true);await b.wait('document.fullscreenElement?.id==="viewport"')}
   await b.evaluate(`${$('display-help')}.open=true;${$('open-settings')}.focus()`);await click('open-settings');assert.equal(await b.evaluate(`${$('settings')}.open&&${$('display-controls')}.hidden`),true);
   await key('Tab','Tab',9);assert.equal(await b.evaluate(`${$('settings')}.contains(document.activeElement)`),true);await key('Escape','Escape',27);await b.wait(`!${$('settings')}.open&&!${$('display-controls')}.hidden`);assert.equal(await b.evaluate(`${$('display-controls')}.hidden`),false);assert.equal(await b.evaluate('document.activeElement.id'),'open-settings');
   if(fullscreen){await b.evaluate('document.exitFullscreen()',true);await b.wait('!document.fullscreenElement')}
  }
  assert.equal(f.requests.length,demoReads);assert.equal(f.source.mutations.length,0);
  // Absent APIs never instantiate synthetic data; invalid sources start neither adapter.
  f.control.capture=false;f.control.cloud='503';await b.navigate(f.origin+'/console.html');await b.wait('ChitonConsoleApp.state.device.capture===false');assert.equal(await b.evaluate('ChitonConsoleApp.source instanceof ChitonConsole.LiveSource'),true);assert.equal(await b.evaluate('ChitonConsoleApp.state.sourceName'),'live');
  await b.command('Page.navigate',{url:f.origin+'/console.html?source=unknown'});await b.wait(`${$('notice')}.textContent.includes('Unknown source')`);assert.equal(await b.evaluate('typeof ChitonConsoleApp'),'undefined');
  assert.deepEqual(b.errors,[]);fs.writeFileSync(path.join(out,'controls.json'),JSON.stringify(report,null,2));console.log('console_display_controls_browser: native mouse/touch/keyboard, draft/scroll/focus/modal/fullscreen, 100 no-work toggles, 30Hz one-flight wiring and default-live/no-write safety passed');
 }finally{await b.close();await f.close();}
})().catch(e=>{console.error(e);process.exitCode=1});
