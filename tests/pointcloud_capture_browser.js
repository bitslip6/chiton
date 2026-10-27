'use strict';
const assert=require('node:assert/strict'),{browserFor}=require('../tools/console-browser-driver');
(async()=>{
 const origin=process.argv[2];if(!origin)throw new Error('Use the local recorded-bundle test server');
 const browser=await browserFor(origin);
 try{
  for(const webgl1 of [false,true]){
   let injection;
   if(webgl1)injection=await browser.command('Page.addScriptToEvaluateOnNewDocument',{source:`const old=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(t,...args){return t==='webgl2'?null:old.call(this,t,...args);};`});
   await browser.navigate(origin+'/');
   if(process.argv.includes('--empty')){
    assert.equal(await browser.evaluate('CaptureViewer.bundle.records.length'),0);
    assert.deepEqual(await browser.evaluate('CaptureViewer.pickRect(0,0,100,100)'),[]);
    assert((await browser.evaluate('CaptureViewer.exportImages()')).directory.includes('export_'));
    if(injection)await browser.command('Page.removeScriptToEvaluateOnNewDocument',{identifier:injection.identifier});
    continue;
   }
   if(process.argv.includes('--large')){
    const result=await browser.evaluate(`(()=>{const c=CaptureViewer,v=c.view;v.setProjection('orthographic');v.setOrientation('front');v.render();const p=new THREE.Vector3(...c.bundle.records[0].slice(0,3)).project(v.camera);return {count:c.bundle.records.length,ids:c.pickRect((p.x+1)*v.canvas.width/2,(1-p.y)*v.canvas.height/2)};})()`);
    assert(result.count>65536);assert.deepEqual(result.ids,[result.count-1],'Equal-depth final visible record ID must survive 24-bit picking');
    if(injection)await browser.command('Page.removeScriptToEvaluateOnNewDocument',{identifier:injection.identifier});
    continue;
   }
   for(const dpr of [1,2]){
    await browser.viewport(1200,1000,dpr);
    await browser.evaluate(`(()=>{const c=CaptureViewer;c.view.resize();c.view.setProjection('orthographic');c.view.setOrientation('front');c.view.render();})()`);
    const picked=await browser.evaluate(`(()=>{const c=CaptureViewer,v=c.view,p=new THREE.Vector3(...c.bundle.records[0].slice(0,3)).project(v.camera);return c.pickRect((p.x+1)*v.canvas.width/2,(1-p.y)*v.canvas.height/2);})()`);
    assert.deepEqual(picked,[0],'Front view depth test must choose record 0, not occluded record 1');
    const visible=await browser.evaluate('CaptureViewer.pickRect(0,0,CaptureViewer.view.canvas.width-1,CaptureViewer.view.canvas.height-1)');
    assert(visible.includes(0));assert(!visible.includes(1),'Occluded record must not be selected by visible rectangle');
    for(const projection of ['orthographic','perspective']){
     await browser.evaluate(`(()=>{const c=CaptureViewer,v=c.view;v.setProjection('${projection}');v.setOrientation('top');v.setPointStyle({shape:'circle',diameterCssPx:8,sizing:'screen'});v.render();})()`);
     const pickedTop=await browser.evaluate(`(()=>{const c=CaptureViewer,v=c.view,p=new THREE.Vector3(...c.bundle.records[1].slice(0,3)).project(v.camera);return c.pickRect((p.x+1)*v.canvas.width/2,(1-p.y)*v.canvas.height/2);})()`);
     assert.deepEqual(pickedTop,[1]);
    }
    // A freely orbited view still maps visible ID fragments, not inverse XYZ.
    await browser.evaluate('CaptureViewer.view.nav.orbit(.3,.2);CaptureViewer.view.render()');
    const orbit=await browser.evaluate('CaptureViewer.pickRect(0,0,CaptureViewer.view.canvas.width-1,CaptureViewer.view.canvas.height-1)');
    assert(orbit.length>0);assert(orbit.every(k=>k>=0&&k<5));
   }
   const clipped=await browser.evaluate(`(()=>{const c=CaptureViewer,v=c.view;v.setProjection('orthographic');v.setOrientation('top');const s=ChitonConsole.defaultClipSettings();s.depth={enabled:true,min:900,max:1100};v.setClipSettings(s);v.render();const p=new THREE.Vector3(...c.bundle.records[1].slice(0,3)).project(v.camera);const found=c.pickRect((p.x+1)*v.canvas.width/2,(1-p.y)*v.canvas.height/2);v.setClipSettings(ChitonConsole.defaultClipSettings());return found;})()`);
   assert.deepEqual(clipped,[],'Picking must use the same clipping predicate as rendering');
   await browser.evaluate('CaptureViewer.selected.clear();CaptureViewer.showSelection();CaptureViewer.view.setProjection("orthographic");CaptureViewer.view.setOrientation("front");CaptureViewer.view.render()');
   await browser.evaluate(`(()=>{const c=CaptureViewer,v=c.view,p=new THREE.Vector3(...c.bundle.records[0].slice(0,3)).project(v.camera);return c.pickRect((p.x+1)*v.canvas.width/2,(1-p.y)*v.canvas.height/2);})()`);
   assert.equal(await browser.evaluate(`(()=>{const p=document.getElementById('raw').getContext('2d').getImageData(0,0,1,1).data;return p[0]>240&&p[2]>150;})()`),true,'Source pixel zero is marked magenta');
   const before=await browser.evaluate('CaptureViewer.pose()');
   const result=await browser.evaluate('CaptureViewer.exportImages()');assert(result.directory.includes('export_'));
   const after=await browser.evaluate('CaptureViewer.pose()');assert.deepEqual(after.navigation,before.navigation);assert.equal(after.projection,before.projection);
   if(injection)await browser.command('Page.removeScriptToEvaluateOnNewDocument',{identifier:injection.identifier});
  }
  assert.deepEqual(browser.errors,[]);
  assert(browser.requests.every(url=>new URL(url).origin===origin),'No device/external network requests');
  console.log('pointcloud_capture_browser: WebGL1/2 ID picking, occlusion, top/front/orbit, DPR, point sizes, exact source marker and paired disk exports PASS');
 }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
