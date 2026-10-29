'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
async function gpuChecks(){
 const C=ChitonConsole,T=THREE,host=document.createElement('div');host.style.cssText='width:256px;height:256px';document.body.append(host);
 const v=new C.CloudView(host);if(v.raf!==null)cancelAnimationFrame(v.raf);v.raf=null;v.dirty=()=>{};v.axes.visible=false;
 const target=new T.WebGLRenderTarget(256,256),camera=new T.OrthographicCamera(-100,100,100,-100,1,10000);camera.position.set(0,0,1000);camera.lookAt(0,0,0);camera.updateMatrixWorld();
 const records=new Int16Array([0,0,0,1500]),config={enabled:1,algorithm:3,evidence_min:250,evidence_max:6000,boundary_gap:200,anchor_tolerance:40,center_margin:60,ramp_residual:30,surface_support:4,center_support:3};
 const snapshot={source:'test',frameType:'depth',revision:1,frameId:5,units:'mm',unitsScope:'calibration_aoi',recordCount:1,records,
  cleanup:{flags:new Uint8Array([8]),config,generation:2,status:2,mixed:1,boundary:0,unclassified:0}};
 v.update(snapshot,{min:0,max:3000});v.setPointStyle({shape:'square',diameterCssPx:10,sizing:'screen'});v.renderer.setClearColor(0x000000);
 const draw=()=>{v.renderer.setRenderTarget(target);v.renderer.render(v.scene,camera);v.checkPrograms();const p=new Uint8Array(4);v.renderer.readRenderTargetPixels(target,128,128,1,1,p);return [...p.slice(0,3)];};
 const same=(a,b,msg)=>{if(JSON.stringify(a)!==JSON.stringify(b))throw new Error(msg+JSON.stringify({a,b}));};
 const baseline=draw(),pose=v.nav.pose(),reference=v.referenceDistance,uploads=v.uploads;
 v.setCleanupMode('highlight',2);const highlight=draw();if(highlight[0]<250||highlight[1]>20||highlight[2]<195)throw new Error('Suspect highlight not magenta');
 v.setCleanupMode('hide',2);same(draw(),[0,0,0],'Hide fragments');if(v.counters().cleanup.matchingAfterCleanup!==0)throw new Error('Combined counts');
 v.setCleanupMode('hide',3);same(draw(),baseline,'Old generation retained');if(v.counters().cleanup.available)throw new Error('Old generation available');
 v.setCleanupMode('hide',2,{...config,boundary_gap:300});same(draw(),baseline,'Same counter with different parameters must retain');
 v.setCleanupMode('hide',2,config);same(draw(),[0,0,0],'Matching full acknowledged configuration hides');
 const copied=v.counters();copied.cleanup.config.boundary_gap=400;if(v.counters().cleanup.config.boundary_gap!==200)throw new Error('Counters leaked mutable applied config');
 v.setCleanupMode('off',2);same(draw(),baseline,'Off restores exact framebuffer');
 const clip=C.defaultClipSettings();clip.depth={enabled:true,min:1000,max:1000};v.setClipSettings(clip);v.setCleanupMode('highlight',2);same(draw(),[0,0,0],'Highlight bypassed clipping');
 v.setClipSettings(C.defaultClipSettings());v.setCleanupMode('hide',2);
 const footer=[],fill=CanvasRenderingContext2D.prototype.fillText;let png;
 CanvasRenderingContext2D.prototype.fillText=function(text,...args){footer.push({text:String(text),width:this.measureText(text).width,limit:this.canvas.width-16});return fill.call(this,text,...args);};
 try{png=await v.png(snapshot,{source:'live',stale:false});}finally{CanvasRenderingContext2D.prototype.fillText=fill;}
 if(!png.size||footer.some(line=>line.width>line.limit))throw new Error('PNG missing or cleanup provenance truncated');
 for(const k of C.CLEANUP_FIELDS)if(!footer.map(x=>x.text).join(' ').includes(k+'='+config[k]))throw new Error('PNG missing applied parameter '+k);
 same(v.nav.pose(),pose,'Cleanup changed pose');if(v.referenceDistance!==reference||v.uploads!==uploads)throw new Error('Policy uploaded/refit geometry');same([...records],[0,0,0,1500],'Original depth changed');
 v.update({...snapshot,cleanup:undefined,frameId:6},null);same(draw(),[153,171,186],'Unflagged upload must retain');
 // ID3 Hide must reject depth as well as color in the real EDL pipeline.
 // Compare with physically absent foreground, keeping identical camera bounds.
 v.setProjection('orthographic');v.nav.target=[0,0,0];v.nav.radius=v.nav.base=1000;v.nav.theta=0;v.nav.phi=Math.PI/2;
 const bounds=C.cloudBounds(new Int16Array([-200,-200,-100,0,200,200,100,0]));
 const paint=()=>{v.nav.bounds=bounds;v.renderer.setRenderTarget(target);v.render();const p=new Uint8Array(256*256*4);v.renderer.readRenderTargetPixels(target,0,0,256,256,p);return [...p];};
 let edlCases=0;
 for(const edl of [false,true])for(const reverse of [false,true]){
  v.setEyeDomeLighting(edl);const rows=[[0,0,100,750],[0,0,-100,250]],flags=[8,1];if(reverse){rows.reverse();flags.reverse();}
  const pair={...snapshot,records:new Int16Array(rows.flat()),recordCount:2,cleanup:{...snapshot.cleanup,flags:new Uint8Array(flags)}};
  v.update(pair,{min:0,max:1000});v.setClipSettings(C.defaultClipSettings());v.setCleanupMode('highlight',2,config);
  const highlighted=paint();if(!highlighted.some((x,i)=>i%4===0&&x>200&&highlighted[i+1]<40&&highlighted[i+2]>180))throw new Error('EDL highlight missing');
  v.setCleanupMode('hide',2,config);const hidden=paint();if(v.edlError||(edl&&!v.eyeDomeLighting))throw new Error('EDL unavailable in test');
  v.update({...pair,records:new Int16Array([0,0,-100,250]),recordCount:1,cleanup:{...snapshot.cleanup,flags:new Uint8Array([1])}},{min:0,max:1000});
  same(paint(),hidden,'ID3 hidden foreground left depth/EDL silhouette');
  v.update(pair,{min:0,max:1000});const onlyFar=C.defaultClipSettings();onlyFar.depth={enabled:true,min:250,max:250};v.setClipSettings(onlyFar);v.setCleanupMode('highlight',2,config);
  same(paint(),hidden,'ID3 highlight bypassed clipping/EDL');edlCases++;
 }
 const result={baseline,highlight,edlCases,cleanup:v.counters().cleanup};target.dispose();v.dispose();host.remove();return result;
}
(async()=>{
 const mock=await fixture();mock.source.mode=4;mock.control.cleanup={records:new Int16Array([0,0,0,1500,50,0,0,1000]),flags:new Uint8Array([8,1])};
 const b=await browserFor(mock.origin),out=path.resolve(process.env.CHITON_CLEANUP_EVIDENCE||'_build/sampled-boundary-cleanup/integration/validation');fs.mkdirSync(out,{recursive:true});
 try{
  await b.navigate(mock.origin+'/console.html?source=live');await b.wait('ChitonConsoleApp.state.cleanup.ack?.generation===1 && ChitonConsoleApp.state.cloud.latest?.cleanup');
  assert.equal(mock.control.cleanup.posts||0,0,'Startup must not mutate settings');
  mock.control.cleanup.freeze=true;
  await b.evaluate(`(()=>{document.getElementById('cleanup-enabled').checked=true;document.getElementById('cleanup-algorithm').value='3';document.getElementById('cleanup-form').dispatchEvent(new Event('input',{bubbles:true}));document.getElementById('cleanup-form').requestSubmit();})()`);
  await b.wait('ChitonConsoleApp.state.cleanup.ack?.generation===2 && !ChitonConsoleApp.state.cleanup.busy');
  assert.equal(mock.control.cleanup.config.algorithm,3);
  assert.equal(await b.evaluate('document.getElementById("cleanup-anchor_tolerance").disabled'),true);
  assert.match(await b.evaluate('document.getElementById("cleanup-policy").textContent'),/radius 6/);
  assert.equal(mock.control.cleanup.config.center_support,3,'Ignored values preserved, not replaced with magic counts');
  await b.evaluate(`(()=>{document.getElementById('cleanup-display').value='hide';document.getElementById('cleanup-display').dispatchEvent(new Event('change'));})()`);
  await b.wait('ChitonConsoleApp.view.cloud?.counters().cleanup.reason.includes("Awaiting")');assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.material.uniforms.cleanupMode.value'),0);
  mock.control.cleanup.freeze=false;await b.wait('ChitonConsoleApp.view.cloud?.counters().cleanup.available && ChitonConsoleApp.view.cloud.counters().cleanup.generation===2');
  assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.counters().cleanup.matchingAfterCleanup'),1);
  const before=await b.evaluate('JSON.stringify(ChitonConsoleApp.view.cloud.nav.pose())');
  await b.evaluate(`(()=>{document.getElementById('cleanup-algorithm').value='0';document.getElementById('cleanup-form').requestSubmit();})()`);
  await b.wait('!document.getElementById("cleanup-error").hidden');assert.equal(mock.control.cleanup.posts,1);assert.equal(await b.evaluate('document.getElementById("cleanup-boundary_gap").value'),'200');
  // A competing operator invalidates the local generation. The pending draft must
  // fail atomically, not silently overwrite their update or retry the mutation.
  await b.evaluate('ChitonConsoleApp.status.stop()');mock.control.cleanup.generation++;
  await b.evaluate(`(()=>{document.getElementById('cleanup-boundary_gap').value='300';document.getElementById('cleanup-form').requestSubmit();})()`);
  await b.wait('!ChitonConsoleApp.state.cleanup.busy && document.getElementById("cleanup-error").textContent.includes("409")');assert.equal(mock.control.cleanup.posts,1);
  await b.evaluate('document.getElementById("cleanup-reload").click()');await b.wait('ChitonConsoleApp.state.cleanup.ack?.generation===3');
  assert.equal(await b.evaluate('JSON.stringify(ChitonConsoleApp.view.cloud.nav.pose())'),before);
  await b.evaluate(`(()=>{document.getElementById('cleanup-boundary_gap').value='400';document.getElementById('cleanup-form').dispatchEvent(new Event('input',{bubbles:true}));})()`);
  mock.control.cleanup.generation++;
  await b.evaluate('(async()=>ChitonConsoleApp.applyStatus(await ChitonConsoleApp.source.readStatus({})))()');
  assert.equal(await b.evaluate('ChitonConsoleApp.state.cleanup.ack.generation'),4);
  assert.equal(await b.evaluate('ChitonConsoleApp.state.cleanup.draftGeneration'),3);
  await b.evaluate('document.getElementById("cleanup-form").requestSubmit()');
  await b.wait('!ChitonConsoleApp.state.cleanup.busy && document.getElementById("cleanup-error").textContent.includes("409")');assert.equal(mock.control.cleanup.posts,1);
  await b.evaluate(`(()=>{document.getElementById('cleanup-boundary_gap').value='400';document.getElementById('cleanup-form').dispatchEvent(new Event('input',{bubbles:true}));})()`);
  mock.control.cleanup.config={...mock.control.cleanup.config,boundary_gap:300}; // observed process/counter reuse
  await b.evaluate('(async()=>ChitonConsoleApp.applyStatus(await ChitonConsoleApp.source.readStatus({})))()');
  assert.equal(await b.evaluate('ChitonConsoleApp.state.cleanup.dirty'),false);
  assert.equal(await b.evaluate('document.getElementById("cleanup-boundary_gap").value'),'300');
  // A status refresh must never overwrite visibly edited fields, even if a
  // browser/autofill integration changed their values without a bubbling input.
  await b.evaluate(`document.getElementById('cleanup-boundary_gap').value='350'`);
  await b.evaluate('(async()=>ChitonConsoleApp.applyStatus(await ChitonConsoleApp.source.readStatus({})))()');
  assert.equal(await b.evaluate('document.getElementById("cleanup-boundary_gap").value'),'350');
  assert.equal(await b.evaluate('ChitonConsoleApp.state.cleanup.draftGeneration'),4);
  async function clickApply(){
   const p=await b.evaluate(`(()=>{document.getElementById('display-controls').hidden=false;document.getElementById('cleanup-details').open=true;const e=document.getElementById('cleanup-apply');e.scrollIntoView();const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
   await b.command('Input.dispatchMouseEvent',{type:'mousePressed',button:'left',clickCount:1,...p});
   await b.command('Input.dispatchMouseEvent',{type:'mouseReleased',button:'left',clickCount:1,...p});
  }
  await clickApply();await b.wait('ChitonConsoleApp.state.cleanup.ack?.generation===5 && !ChitonConsoleApp.state.cleanup.busy');
  assert.equal(mock.control.cleanup.config.boundary_gap,350);
  assert.equal(new URL(b.requests.filter(u=>u.includes('/api/cleanup/v1/config?')).at(-1)).searchParams.get('boundary_gap'),'350');
  assert.equal(await b.evaluate('document.getElementById("cleanup-boundary_gap").value'),'350');
  // Exercise real keyboard editing and real pointer submission, not just .value
  // assignment + requestSubmit, for every numeric field and the other controls.
  const draft={enabled:0,algorithm:1,evidence_min:6500,evidence_max:200,boundary_gap:1,
   anchor_tolerance:65535,center_margin:0,ramp_residual:65535,surface_support:0,center_support:255};
  await b.evaluate(`document.getElementById('cleanup-enabled').click();document.getElementById('cleanup-algorithm').value='1';document.getElementById('cleanup-algorithm').dispatchEvent(new Event('change',{bubbles:true}));`);
  for(const [k,v] of Object.entries(draft).slice(2)){
   await b.evaluate(`(()=>{const e=document.getElementById('cleanup-${k}');e.scrollIntoView();e.focus();e.select();})()`);
   await b.command('Input.insertText',{text:String(v)});
  }
  await b.evaluate('(async()=>ChitonConsoleApp.applyStatus(await ChitonConsoleApp.source.readStatus({})))()');
  await clickApply();await b.wait('ChitonConsoleApp.state.cleanup.ack?.generation===6 && !ChitonConsoleApp.state.cleanup.busy');
  assert.deepEqual(mock.control.cleanup.config,draft);
  const submitted=new URL(b.requests.filter(u=>u.includes('/api/cleanup/v1/config?')).at(-1)).searchParams;
  for(const [k,v] of Object.entries(draft))assert.equal(submitted.get(k),String(v),k+' must send visible draft');
  await b.evaluate('(async()=>ChitonConsoleApp.applyStatus(await ChitonConsoleApp.source.readStatus({})))()');
  assert.equal(await b.evaluate('document.getElementById("cleanup-boundary_gap").value'),'1');
  // Discovering an eventless draft during a competing operator's refresh must
  // not silently rebase that draft onto the newer generation.
  await b.evaluate(`document.getElementById('cleanup-boundary_gap').value='300'`);
  mock.control.cleanup.generation++;
  await b.evaluate('(async()=>ChitonConsoleApp.applyStatus(await ChitonConsoleApp.source.readStatus({})))()');
  assert.equal(await b.evaluate('document.getElementById("cleanup-boundary_gap").value'),'300');
  assert.equal(await b.evaluate('ChitonConsoleApp.state.cleanup.draftGeneration'),6);
  const posts=mock.control.cleanup.posts;
  await clickApply();await b.wait('!ChitonConsoleApp.state.cleanup.busy && document.getElementById("cleanup-error").textContent.includes("409")');
  assert.equal(mock.control.cleanup.posts,posts);
  assert.equal(mock.control.cleanup.config.boundary_gap,1);
  // All-zero tuning is accepted by native form validation and the API; no
  // inferred minimum support, positive tolerance or ordered interval remains.
  await b.evaluate(`(()=>{for(const k of ChitonConsole.CLEANUP_FIELDS.slice(2))document.getElementById('cleanup-'+k).value=0;document.getElementById('cleanup-form').dispatchEvent(new Event('input',{bubbles:true}));})()`);
  assert.equal(await b.evaluate('document.getElementById("cleanup-form").checkValidity()'),true);
  await clickApply();await b.wait('ChitonConsoleApp.state.cleanup.ack?.generation===8 && !ChitonConsoleApp.state.cleanup.busy');
  for(const k of Object.keys(draft).slice(2))assert.equal(mock.control.cleanup.config[k],0,k);
  // Old servers must not expose an unadvertised mode. No automatic config write.
  mock.control.cleanup.algorithms=[1,2];
  await b.evaluate('document.getElementById("cleanup-reload").click()');
  await b.wait('!ChitonConsoleApp.state.cleanup.busy');
  assert.equal(await b.evaluate('document.querySelector("#cleanup-algorithm option[value=\\"3\\"]").disabled'),true);
  assert.equal(await b.evaluate('document.querySelector("#cleanup-algorithm option[value=\\"3\\"]").hidden'),true);
  const reports=[];
  for(const webgl1 of [false,true]){
   let injection;if(webgl1){injection=await b.command('Page.addScriptToEvaluateOnNewDocument',{source:`const original=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(t,...a){return t==='webgl2'?null:original.call(this,t,...a);};`});await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.evaluate('ChitonConsoleApp.prepareCapture({cloudMode:true})');}
   reports.push(await b.evaluate(`(${gpuChecks.toString()})()`));if(injection)await b.command('Page.removeScriptToEvaluateOnNewDocument',{identifier:injection.identifier});
  }
  fs.writeFileSync(path.join(out,'cleanup-browser.json'),JSON.stringify(reports,null,2));
  console.log('console_cleanup_browser: acknowledged tuning, atomic validation/stale writes, pending generations, WebGL1/2 highlight/hide/clipping, pose/XYZD preservation and PNG passed');
 }finally{await b.close();await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
