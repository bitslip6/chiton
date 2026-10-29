#!/usr/bin/env node
/* Synthetic loopback-only benchmark; GPU completion readback is test-only. */
'use strict';
const fs=require('node:fs'),http=require('node:http'),path=require('node:path'),os=require('node:os');
const {browserFor}=require('./console-browser-driver');
async function benchmark(candidate,wantedCount,wantedEdl,wantedName,wantedRound){
 const C=ChitonConsole,host=document.getElementById('host'),v=new C.CloudView(host);if(v.raf!==null)cancelAnimationFrame(v.raf);v.raf=null;v.dirty=()=>{};v.axes.visible=false;
 const gl=v.renderer.getContext(),read=new Uint8Array(4),complete=()=>{gl.finish();gl.readPixels(400,300,1,1,gl.RGBA,gl.UNSIGNED_BYTE,read);};
 const uploads=[];for(const name of ['bufferData','bufferSubData']){const old=gl[name].bind(gl);gl[name]=(...a)=>{if(a[0]===gl.ARRAY_BUFFER){const data=a[name==='bufferData'?1:2];uploads.push({name,bytes:typeof data==='number'?data:(a[4]||data.length)*data.BYTES_PER_ELEMENT});}return old(...a);};}
 const runs=[],warmup=4,samples=10;
 for(const count of [wantedCount]){
  const records=new Int16Array(count*4);for(let i=0;i<count;i++){const x=Math.round(-10000+(i%1000)/999*20000),y=Math.round(-10000+Math.floor(i/1000)/Math.ceil(count/1000)*20000);records.set([x,y,((i*17)%1000)-500,x],i*4);}
  const snapshot={source:'benchmark',revision:1,frameType:'depth',units:'mm',unitsScope:'calibration_aoi',records,recordCount:count,frameId:1};v.update(snapshot,{min:-10000,max:10000});v.nav.target=[0,0,0];v.nav.radius=v.nav.base=40000;v.nav.theta=.35;v.nav.phi=1.25;v.setPointStyle({shape:'circle',diameterCssPx:2.5,sizing:'screen'});
  const cases=[{name:'off',settings:candidate?C.defaultClipSettings():null}];if(candidate)for(const kind of ['depth','box','intersection'])for(const fraction of [0,.1,.5,1]){
   const s=C.defaultClipSettings(),min=fraction===0?32767:-32768,max=fraction===0?32767:fraction===1?32767:Math.floor(-10000+fraction*20000);
   if(kind!=='box')s.depth={enabled:true,min,max};if(kind!=='depth'){s.box.enabled=true;s.box.min[0]=min;s.box.max[0]=max;}cases.push({name:kind+'-'+fraction,settings:s});
  }
  for(const round of [wantedRound])for(const edl of round?[true,false]:[false,true])for(const c of round?[...cases].reverse():cases){
   if(edl!==wantedEdl||c.name!==wantedName)continue;
   v.setEyeDomeLighting(edl);if(candidate)v.setClipSettings(c.settings);v.render();complete();if(edl&&!v.eyeDomeLighting)throw new Error('EDL unsupported');const rows=[];
   const startUploads=uploads.length;
   for(let i=0;i<warmup+samples;i++){
    let a=performance.now();if(candidate)C.summarizeCloudClipping(records,c.settings);let b=performance.now();const countPreparation=b-a;
    if(candidate){const other=C.validateClipSettings(c.settings);other.depth.min=other.depth.min===32767?32766:other.depth.min+1;v.setClipSettings(other);}a=performance.now();if(candidate)v.setClipSettings(c.settings);b=performance.now();const apply=b-a;
    a=performance.now();snapshot.frameId++;v.update(snapshot,{min:-10000,max:10000});b=performance.now();v.render();const submitted=performance.now();complete();const done=performance.now();
    v.nav.orbit(i%2?.001:-.001,0);const d=performance.now();v.render();complete();const e=performance.now();
    if(i>=warmup)rows.push({countPreparation,apply,prepare:b-a,submit:submitted-b,completedDraw:done-b,interaction:e-d});
   }
   const framebuffer=new Uint8Array(v.canvas.width*v.canvas.height*4);gl.readPixels(0,0,v.canvas.width,v.canvas.height,gl.RGBA,gl.UNSIGNED_BYTE,framebuffer);let pixelHash=2166136261;for(const byte of framebuffer)pixelHash=Math.imul(pixelHash^byte,16777619);
   runs.push({candidate,count,round,edl,name:c.name,rows,counters:v.counters(),pixelHash:pixelHash>>>0,observedUploads:uploads.slice(startUploads)});
  }
 }
 const environment={webgl:gl.getParameter(gl.VERSION),renderer:gl.getParameter(gl.getExtension('WEBGL_debug_renderer_info').UNMASKED_RENDERER_WEBGL),dpr:v.effectivePixelRatio,buffer:[v.canvas.width,v.canvas.height]};v.dispose();return {runs,warmup,samples,environment};
}
async function benchmarkCountFeed(count){
 const C=ChitonConsole,records=new Int16Array(count*4);for(let i=0;i<count;i++)records.set([i%20000-10000,i%20000-10000,i%20000-10000,i%20000-10000],i*4);
 const cases=[];for(const kind of ['off','depth','box','intersection']){const s=C.defaultClipSettings();if(kind==='depth'||kind==='intersection')s.depth={enabled:true,min:-32768,max:0};if(kind==='box'||kind==='intersection'){s.box.enabled=true;s.box.max[0]=0;}cases.push({kind,settings:s});}
 const runs=[];for(const c of cases){const start=performance.now(),rows=[];for(let i=0;i<24;i++){await new Promise(r=>setTimeout(r,Math.max(0,start+i*100-performance.now())));const a=performance.now(),summary=C.summarizeCloudClipping(records,c.settings),b=performance.now();if(i>=4)rows.push(b-a);if(summary.submittedCount!==count)throw new Error('feed count mismatch');}runs.push({kind:c.kind,count,targetHz:10,elapsedMs:performance.now()-start,scanMs:rows});}return runs;
}
(async()=>{
 const out=path.resolve(process.argv[2]||'_build/pointcloud-clipping/validation/benchmark.json'),sockets=new Set();
 const dirs={baseline:path.resolve('_build/pointcloud-clipping/baseline/files'),candidate:path.resolve('release/files')};
 const server=http.createServer((req,res)=>{const parts=req.url.split('/'),dir=dirs[parts[1]],name=parts[2];if(!dir||!name||!/^[a-z.-]+$/.test(name)){res.writeHead(404);res.end();return;}
  if(name==='smoke.html'){res.setHeader('Content-Type','text/html');res.end('<!doctype html><div id="host" style="width:800px;height:600px"></div>'+['three.min.js','console-model.js','console-cloud.js','console-cloud-material.js','console-cloud-view.js'].map(n=>`<script src="/${parts[1]}/${n}"></script>`).join('')+'<script>window.CONSOLE_READY=true</script>');return;}
  const file=path.join(dir,name);if(!fs.existsSync(file)){res.writeHead(404);res.end();return;}res.setHeader('Content-Type','application/javascript');res.end(fs.readFileSync(file));});
 server.on('connection',s=>{sockets.add(s);s.on('close',()=>sockets.delete(s));});await new Promise(r=>server.listen(0,'127.0.0.1',r));const origin='http://127.0.0.1:'+server.address().port,b=await browserFor(origin);
 try{await b.viewport(800,600,1);const result={versions:[],browser:await b.command('Browser.getVersion',{},null),cpu:os.cpus()[0]?.model};
  result.versions=[{version:'baseline',runs:[]},{version:'candidate',runs:[]}];
  for(const round of [0,1])for(const version of round?['candidate','baseline']:['baseline','candidate']){await b.navigate(origin+'/'+version+'/smoke.html');const data=result.versions.find(v=>v.version===version);
   const names=['off'];if(version==='candidate'&&!process.env.CHITON_CLIP_BENCH_OFF_ONLY)for(const kind of ['depth','box','intersection'])for(const p of [0,.1,.5,1])names.push(kind+'-'+p);
   for(const count of [40000,307200])for(const edl of round?[true,false]:[false,true])for(const name of round?[...names].reverse():names){const batch=await b.evaluate(`(${benchmark.toString()})(${version==='candidate'},${count},${edl},${JSON.stringify(name)},${round})`);data.runs.push(...batch.runs);data.environment=batch.environment;data.warmup=batch.warmup;data.samples=batch.samples;}
  }
  const q=(a,p)=>[...a].sort((x,y)=>x-y)[Math.min(a.length-1,Math.floor(a.length*p))];result.summary=[];
  for(const v of result.versions)for(const count of [40000,307200])for(const edl of [false,true])for(const name of [...new Set(v.runs.map(r=>r.name))]){const rows=v.runs.filter(r=>r.count===count&&r.edl===edl&&r.name===name).flatMap(r=>r.rows),timings={};for(const key of Object.keys(rows[0]))timings[key]={median:q(rows.map(r=>r[key]),.5),p95:q(rows.map(r=>r[key]),.95)};result.summary.push({version:v.version,count,edl,name,timings});}
  for(const count of [40000,307200])for(const edl of [false,true])for(const round of [0,1]){const select=version=>result.versions.find(v=>v.version===version).runs.find(r=>r.count===count&&r.edl===edl&&r.round===round&&r.name==='off');if(select('baseline').pixelHash!==select('candidate').pixelHash)throw new Error('Clipping-off baseline framebuffer mismatch');}
  if(!process.env.CHITON_CLIP_BENCH_OFF_ONLY){await b.navigate(origin+'/candidate/smoke.html');result.synthetic10Hz=[];for(const count of [40000,307200])result.synthetic10Hz.push(...await b.evaluate(`(${benchmarkCountFeed.toString()})(${count})`));}
  result.notes='800x600 DPR1 circle2.5 fixed synthetic input/pose/range. Two reversed case-order rounds; test-only finish+readPixels. Count preparation is a separate CPU reference scan; production scans only on frame/applied change. Geometry submission/HTTP bytes unchanged. Software WebGL is not physical GPU qualification.';
  fs.mkdirSync(path.dirname(out),{recursive:true});fs.writeFileSync(out,JSON.stringify(result,null,2));console.log(JSON.stringify(result.summary,null,2));if(b.errors.length)throw new Error(JSON.stringify(b.errors));
 }finally{await b.close();for(const s of sockets)s.destroy();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
