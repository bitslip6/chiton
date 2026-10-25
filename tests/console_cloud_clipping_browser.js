/* Actual WebGL1/2 + console UI against a disposable loopback fixture only. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
async function gpuChecks(){
 const C=ChitonConsole,T=THREE,check=(v,m)=>{if(!v)throw new Error(m);},same=(a,b,m)=>check(JSON.stringify(a)===JSON.stringify(b),m);
 const host=document.createElement('div');host.style.cssText='position:fixed;width:256px;height:256px';document.body.append(host);
 const v=new C.CloudView(host);if(v.raf!==null)cancelAnimationFrame(v.raf);v.raf=null;v.dirty=()=>{};v.axes.visible=false;
 const gl=v.renderer.getContext(),uploads=[];for(const name of ['bufferData','bufferSubData']){const old=gl[name].bind(gl);gl[name]=(...args)=>{if(args[0]===gl.ARRAY_BUFFER){const a=args[name==='bufferData'?1:2];uploads.push({name,bytes:typeof a==='number'?a:(args[4]||a.length)*a.BYTES_PER_ELEMENT});}return old(...args);};}
 const clear=[37,53,71];v.renderer.setClearColor(new T.Color(...clear.map(x=>x/255)));
 const target=new T.WebGLRenderTarget(256,256,{minFilter:T.NearestFilter,magFilter:T.NearestFilter}),camera=new T.OrthographicCamera(-40000,40000,40000,-40000,1,200000);
 camera.position.set(0,0,100000);camera.lookAt(0,0,0);camera.updateMatrixWorld();
 let frame=0;const snap=records=>({records:new Int16Array(records),recordCount:records.length/4,frameId:++frame,source:'test',frameType:'depth',revision:1,units:'mm',unitsScope:'calibration_aoi'});
 const draw=()=>{v.renderer.setRenderTarget(target);v.renderer.render(v.scene,camera);v.checkPrograms();};
 const pixel=(x=128,y=128)=>{const p=new Uint8Array(4);v.renderer.readRenderTargetPixels(target,x,y,1,1,p);return [...p.slice(0,3)];};
 const equalColor=(got,expected,m)=>check(got.every((x,k)=>Math.abs(x-expected[k])<=2),m+JSON.stringify({got,expected}));
 const masks=[C.defaultClipSettings()];for(const d of [-32768,-1,0,1,32767]){const s=C.defaultClipSettings();s.depth={enabled:true,min:d,max:d};masks.push(s);}
 const depth=C.defaultClipSettings();depth.depth={enabled:true,min:-1,max:1};masks.push(depth);
 for(let k=0;k<3;k++)for(const interval of [[-32768,-32768],[-1,1],[32767,32767]]){const s=C.defaultClipSettings();s.box.enabled=true;s.box.min[k]=interval[0];s.box.max[k]=interval[1];masks.push(s);}
 const intersection=C.defaultClipSettings();intersection.depth={enabled:true,min:-1,max:1};intersection.box={enabled:true,min:[-1,-1,-1],max:[1,1,1]};masks.push(intersection);
 const records=[[-32768,0,1,32767],[-1,1,0,-1],[0,0,0,0],[1,-1,-1,1],[32767,0,-32768,-32768],[0,32767,32767,0],[0,-32768,0,32767]];
 let parity=0;v.setPointStyle({shape:'square',diameterCssPx:10,sizing:'screen'});
 for(const r of records){const s=snap(r);v.update(s,{min:-32768,max:32767});camera.position.set(r[0],r[1],100000);camera.lookAt(r[0],r[1],0);camera.updateMatrixWorld();
  for(const mask of masks){v.setClipSettings(mask);draw();const n=C.summarizeCloudClipping(s.records,mask).matchingCount;
   check(v.counters().matchingCount===n,'cached count parity');equalColor(pixel(),n?C.cloudColor(r[3],v.range).map(x=>x*255):clear,'signed inclusive GPU parity');parity++;
  }
 }
 // Large rejected sprites never leak, for all view directions/projections/sizing/DPR paths.
 const none=C.defaultClipSettings();none.depth={enabled:true,min:1,max:1};
 const fixed=snap([0,0,0,0]);v.update(fixed,{min:0,max:1000});v.setClipSettings(none);
 for(const projection of ['perspective','orthographic'])for(const orientation of ['front','top','side'])for(const shape of ['circle','square'])for(const size of [1,10])for(const raw of [1,2,8]){
  Object.defineProperty(window,'devicePixelRatio',{configurable:true,value:raw});v.resize();v.setProjection(projection);v.setOrientation(orientation);v.setPointStyle({shape,diameterCssPx:size,sizing:'perspective'});v.renderer.setRenderTarget(target);v.render();
  const p=new Uint8Array(256*256*4);v.renderer.readRenderTargetPixels(target,0,0,256,256,p);for(let i=0;i<p.length;i+=4)check(p[i]===clear[0]&&p[i+1]===clear[1]&&p[i+2]===clear[2],'rejected sprite leaked fragment');
 }
 Object.defineProperty(window,'devicePixelRatio',{configurable:true,value:1});v.resize();v.setPointStyle({shape:'square',diameterCssPx:10,sizing:'screen'});camera.position.set(0,0,100000);camera.lookAt(0,0,0);camera.updateMatrixWorld();
 const nearMask=C.defaultClipSettings();nearMask.depth={enabled:true,min:250,max:250};
 for(const reverse of [false,true]){const rows=[[0,0,32767,750],[0,0,-32768,250]];if(reverse)rows.reverse();v.update(snap(rows.flat()),{min:0,max:1000});v.setClipSettings(nearMask);draw();equalColor(pixel(),[0,0,255],'reject near reveals far, both orders');}
 // The same point selection feeds EDL color AND depth. Compare to a physically absent foreground.
 v.setProjection('orthographic');v.nav.target=[0,0,0];v.nav.radius=v.nav.base=1000;v.nav.theta=0;v.nav.phi=Math.PI/2;
 const fixedBounds=C.cloudBounds(new Int16Array([-200,-200,-100,0,200,200,100,0]));
 const paint=()=>{v.nav.bounds=fixedBounds;v.renderer.setRenderTarget(target);v.render();const p=new Uint8Array(256*256*4);v.renderer.readRenderTargetPixels(target,0,0,256,256,p);return [...p];};
 const images={};for(const edl of [false,true])for(const reverse of [false,true]){
  v.setEyeDomeLighting(edl);const rows=[[0,0,100,750],[0,0,-100,250]];if(reverse)rows.reverse();v.update(snap(rows.flat()),{min:0,max:1000});v.setClipSettings(nearMask);const clipped=paint();check(!v.edlError,'EDL supported in test');
  v.update(snap([0,0,-100,250]),{min:0,max:1000});v.setClipSettings(C.defaultClipSettings());same(paint(),clipped,'no rejected foreground depth or EDL silhouette');
  const c=document.createElement('canvas');c.width=c.height=256;c.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(clipped),256,256),0,0);images[(edl?'edl':'plain')+'-'+reverse]=c.toDataURL().split(',')[1];
 }
 // Real uploads, cached count reads, frozen settings, and zero -> subset -> all recovery.
 v.setEyeDomeLighting(false);v.setClipSettings(none);const max=new Int16Array(C.CLOUD_MAX*4);max[3]=250;
 v.update({...snap([]),records:max,recordCount:C.CLOUD_MAX},null);draw();const start=uploads.length,version=v.attributeBuffer.version,pose=v.nav.pose(),ref=v.referenceDistance,programs=v.renderer.info.programs.length;
 for(const s of [nearMask,C.defaultClipSettings(),none]){v.setClipSettings(s);draw();}
 check(uploads.length===start&&v.attributeBuffer.version===version,'settings-only real GL upload invariant');same(v.nav.pose(),pose,'clipping preserves camera');check(v.referenceDistance===ref,'clipping preserves size reference');
 const original=C.summarizeCloudClipping;C.summarizeCloudClipping=()=>{throw new Error('count rescan on cheap reads');};try{for(let i=0;i<10;i++){v.counters();v.nav.orbit(.001,0);draw();}v.setClipSettings(none);}finally{C.summarizeCloudClipping=original;}
 v.update(snap([0,0,0,0]),null);draw();check(uploads.length===start+1&&uploads.at(-1).bytes===8,'new nonempty frame while zero matches uploads prefix');check(v.points.visible&&v.geometry.drawRange.count===1,'zero matches still submitted');
 const before=uploads.length;v.setClipSettings(C.defaultClipSettings());draw();check(uploads.length===before,'zero match recovery uniform-only');
 v.update(snap([]),null);draw();check(uploads.length===before&&v.counters().matchingCount===0,'empty clears');v.update(snap([0,0,0,0]),null);draw();check(uploads.length===before+1,'empty recovery');
 check(v.renderer.info.programs.length===programs,'no clipping program variants');
 const saved=v.counters();saved.clipSettings.box.min[0]=17;check(v.clipSettings.box.min[0]===-32768,'copied counters');
 // Off restores exact framebuffer. No source mutation.
 const source=snap([0,0,0,500]),owned=[...source.records];v.update(source,{min:0,max:1000});v.setClipSettings(C.defaultClipSettings());draw();const baseline=pixel();v.setClipSettings(none);draw();v.setClipSettings(C.defaultClipSettings());draw();same(pixel(),baseline,'off restores baseline pixels');same([...source.records],owned,'source unchanged');
 const resources={geometries:v.renderer.info.memory.geometries,textures:v.renderer.info.memory.textures,programs:v.renderer.info.programs.length},uploadCount=uploads.length;
 for(let i=0;i<50;i++){v.setClipSettings(i%2?none:C.defaultClipSettings());draw();}
 same({geometries:v.renderer.info.memory.geometries,textures:v.renderer.info.memory.textures,programs:v.renderer.info.programs.length},resources,'repeated apply/reset resource reuse');check(uploads.length===uploadCount,'repeated clipping never uploads');
 const result={webgl:gl.getParameter(gl.VERSION),parity,uploads,programs,counters:v.counters(),images};target.dispose();v.dispose();host.remove();return result;
}
(async()=>{
 const mock=await fixture(),b=await browserFor(mock.origin),out=path.resolve('_build/pointcloud-clipping');fs.mkdirSync(path.join(out,'validation'),{recursive:true});fs.mkdirSync(path.join(out,'screenshots'),{recursive:true});
 const reports=[];try{
  for(const webgl1 of [false,true]){let injection;if(webgl1)injection=await b.command('Page.addScriptToEvaluateOnNewDocument',{source:`const old=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(t,...a){return t==='webgl2'?null:old.call(this,t,...a);};`});
   await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.evaluate('ChitonConsoleApp.prepareCapture({cloudMode:true})');const report=await b.evaluate(`(${gpuChecks.toString()})()`);
   assert.match(report.webgl,webgl1?/WebGL 1.0/:/WebGL 2.0/);for(const [name,p] of Object.entries(report.images))fs.writeFileSync(path.join(out,'screenshots',`${webgl1?'webgl1':'webgl2'}-${name}.png`),Buffer.from(p,'base64'));delete report.images;reports.push(report);
   if(injection)await b.command('Page.removeScriptToEvaluateOnNewDocument',{identifier:injection.identifier});
  }
  fs.writeFileSync(path.join(out,'validation/gpu.json'),JSON.stringify(reports,null,2));
  console.log('console_cloud_clipping_browser: actual WebGL1/2 signed predicate parity, no-fragment/depth rejection, EDL, uniform-only GL uploads, zero/new-frame recovery and cached counts passed');
 }finally{await b.close();await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
