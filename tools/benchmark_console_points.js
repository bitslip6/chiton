#!/usr/bin/env node
/* Loopback-only, deterministic renderer qualification. Never loads an API or device source.
 * Baseline overrides are an explicit allowlist of preserved files; runtime is never changed.
 * finish() is test-only and separately reported from CPU submission; no runtime synchronization.
 */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),http=require('node:http'),path=require('node:path'),crypto=require('node:crypto'),os=require('node:os');
const {browserFor}=require('./console-browser-driver');
const root=path.resolve(__dirname,'..'),evidence=path.join(root,'_build/pointcloud-renderer-style'),out=path.resolve(process.argv[2]||path.join(evidence,'validation/benchmark'));
const assets=['three.min.js','console-model.js','console-cloud.js','console-cloud-material.js','console-cloud-view.js'];
const baseline=['console-cloud.js','console-cloud-view.js'];
async function measure({count,style,warmup,samples,representation='current',distanceScale=1}){
    const C=ChitonConsole,v=new C.CloudView(document.getElementById('host'));
    if(v.raf!=null)cancelAnimationFrame(v.raf);v.raf=null;v.dirty=()=>{};v.axes.visible=false;
    if(v.setPointStyle)v.setPointStyle(style);
    // Diagnostic only: same runtime shader/update, replaced attribute storage before first upload.
    if(representation!=='current'){
        const length=(representation==='count-short'?count:C.CLOUD_MAX)*4;
        v.records=representation==='capacity-float'?new Float32Array(length):new Int16Array(length);
        v.attributeBuffer=new THREE.InterleavedBuffer(v.records,4).setUsage(THREE.DynamicDrawUsage);
        v.geometry.setAttribute('position',new THREE.InterleavedBufferAttribute(v.attributeBuffer,3,0,false));
        v.geometry.setAttribute('depth',new THREE.InterleavedBufferAttribute(v.attributeBuffer,1,3,false));
    }
    const records=new Int16Array(count*4);
    // Fixed asymmetric, dense surface with deterministic depths. Same records/pose/range for all cases.
    for(let i=0;i<count;i++){const x=i%640,y=Math.floor(i/640);records.set([(x-320)*6,(y-240)*6,((x*17+y*11)%1000)-500,200+((i*37)%2600)],i*4);}
    const snapshot={source:'benchmark',revision:1,frameType:'depth',units:'depth_sensor_units',unitsScope:'sensor',recordCount:count,records,frameId:1};
    const range={min:0,max:3000},gl=v.renderer.getContext();let observed=[];
    for(const method of ['bufferData','bufferSubData']){const original=gl[method].bind(gl);gl[method]=(...a)=>{
        if(a[0]===gl.ARRAY_BUFFER){const data=a[method==='bufferData'?1:2],bytes=typeof data==='number'?data:(a[4]??data.length)*data.BYTES_PER_ELEMENT;observed.push({method,bytes});}
        return original(...a);
    };}
    v.update(snapshot,range);v.nav.target=[0,0,0];v.nav.radius=7000*distanceScale;v.nav.base=7000;v.nav.theta=.35;v.nav.phi=1.25;
    if(v.material.uniforms){v.referenceDistance=7000;v.material.uniforms.referenceDistance.value=7000;}
    v.render();gl.finish();const preflight=new Uint8Array(800*600*4);gl.readPixels(0,0,800,600,gl.RGBA,gl.UNSIGNED_BYTE,preflight);let coveredPixels=0;for(let i=0;i<preflight.length;i+=4)if(preflight[i]!==3||preflight[i+1]!==8||preflight[i+2]!==13)coveredPixels++;if(!coveredPixels||v.renderer.info.render.points!==count)throw new Error('Benchmark did not draw actual point pixels');const initialUploads=observed;observed=[];
    const readback=new Uint8Array(4),complete=()=>{gl.finish();gl.readPixels(400,300,1,1,gl.RGBA,gl.UNSIGNED_BYTE,readback);};
    const rows=[];
    for(let i=0;i<warmup+samples;i++){
        snapshot.frameId++;const t0=performance.now();v.update(snapshot,range);const t1=performance.now();
        v.render();const t2=performance.now();complete();const t3=performance.now(),updateUploadCount=observed.length;
        v.nav.orbit(i%2?.002:-.002,0);const t4=performance.now();v.render();const t5=performance.now();complete();const t6=performance.now();
        // Recolor and style switches are interactive uniform work in the candidate, RGB uploads in baseline.
        const u0=performance.now();v.recolor(snapshot,{min:0,max:i%2?3001:3000});if(v.setPointStyle)v.setPointStyle({...style});const u1=performance.now();
        v.render();const u2=performance.now();complete();const u3=performance.now();
        if(i>=warmup)rows.push({prepareMs:t1-t0,renderSubmitMs:t2-t1,renderCompleteMs:t3-t1,orbitSubmitMs:t5-t4,orbitCompleteMs:t6-t4,recolorPrepareMs:u1-u0,recolorSubmitMs:u2-u1,recolorCompleteMs:u3-u1,uploads:observed,uploadPhases:{update:observed.slice(0,updateUploadCount),recolor:observed.slice(updateUploadCount)}});
        observed=[];
    }
    const counters=v.counters(),environment={glVersion:gl.getParameter(gl.VERSION),renderer:gl.getParameter(gl.RENDERER),unmaskedRenderer:gl.getExtension('WEBGL_debug_renderer_info')?gl.getParameter(gl.getExtension('WEBGL_debug_renderer_info').UNMASKED_RENDERER_WEBGL):null,pointSizeRange:[...gl.getParameter(gl.ALIASED_POINT_SIZE_RANGE)],three:THREE.REVISION};
    if(gl.getError()!==gl.NO_ERROR)throw new Error('Benchmark WebGL error');
    const result={count,style,representation,distanceScale,warmup,samples,initialUploads,counters,environment,coveredPixels,submittedPoints:v.renderer.info.render.points,readback:[...readback],dataSha256:[...new Uint8Array(await crypto.subtle.digest('SHA-256',records))].map(x=>x.toString(16).padStart(2,'0')).join(''),rows};v.dispose();return result;
}
(async()=>{
    fs.mkdirSync(out,{recursive:true});
    const diagnostic=process.argv[3]==='--diagnostic';
    const server=http.createServer((req,res)=>{const u=new URL(req.url,'http://127.0.0.1'),name=u.pathname.slice(1),old=u.searchParams.get('baseline')==='1';
        if(name==='bench.html'){res.setHeader('Content-Type','text/html; charset=utf-8');res.end('<!doctype html><style>body{margin:0}#host{width:800px;height:600px}</style><div id="host"></div>'+assets.filter(n=>!old||n!=='console-cloud-material.js').map(n=>`<script src="/${n}?baseline=${old?1:0}"></script>`).join('')+'<script>window.CONSOLE_READY=true;</script>');return;}
        if(!assets.includes(name)){res.writeHead(404);res.end();return;}
        res.setHeader('Content-Type','application/javascript; charset=utf-8');res.end(fs.readFileSync(path.join(old&&baseline.includes(name)?path.join(evidence,'baseline'):path.join(root,'release/files'),name)));
    });await new Promise(r=>server.listen(0,'127.0.0.1',r));const origin='http://127.0.0.1:'+server.address().port,b=await browserFor(origin);
    const report={date:new Date().toISOString(),node:process.version,platform:process.platform,kernel:os.release(),cpu:os.cpus()[0]?.model,browser:await b.command('Browser.getVersion',{},null),warmup:process.argv[3]==='--extremes'?5:8,samples:process.argv[3]==='--extremes'?10:20,rounds:3,notes:'CPU submission and test-only gl.finish + one-pixel readback completion are separate. No forced synchronization in runtime. No HTTP cloud bytes measured/changed. Fixed 800x600 DPR1, range 0..3000, radius7000 target0 theta.35 phi1.25. Software timing is not physical GPU performance.',hashes:{},runs:[]};
    for(const [label,dir,names] of [['baseline',path.join(evidence,'baseline'),baseline],['candidate',path.join(root,'release/files'),assets]])for(const n of names)report.hashes[label+'/'+n]=crypto.createHash('sha256').update(fs.readFileSync(path.join(dir,n))).digest('hex');
    try{
        await b.viewport(800,600,1);
        for(let round=0;round<report.rounds;round++)for(const count of [40000,307200]){
            // Reverse equal-style order in alternate rounds to expose thermal/order noise.
            const cases=[{label:'old-square-2.5',old:true,style:{shape:'square',diameterCssPx:2.5,sizing:'screen'}},{label:'new-square-2.5',style:{shape:'square',diameterCssPx:2.5,sizing:'screen'}}];
            if(round%2)cases.reverse();
            if(process.argv[3]==='--extremes'){cases.length=0;for(const distanceScale of [.5,2])cases.push({label:'new-circle-10-perspective-distance-'+distanceScale,distanceScale,style:{shape:'circle',diameterCssPx:10,sizing:'perspective'}});}
            else if(diagnostic)cases.push({label:'diagnostic-count-short',representation:'count-short',style:{shape:'square',diameterCssPx:2.5,sizing:'screen'}},{label:'diagnostic-capacity-float',representation:'capacity-float',style:{shape:'square',diameterCssPx:2.5,sizing:'screen'}});
            else cases.push(...[{label:'new-circle-2.5',shape:'circle',diameterCssPx:2.5,sizing:'screen'},{label:'new-square-1',shape:'square',diameterCssPx:1,sizing:'screen'},{label:'new-circle-10',shape:'circle',diameterCssPx:10,sizing:'screen'},{label:'new-circle-10-perspective',shape:'circle',diameterCssPx:10,sizing:'perspective'}].map(({label,...style})=>({label,style})));
            for(const c of cases){await b.navigate(origin+'/bench.html?baseline='+(c.old?1:0));const result=await b.evaluate(`(${measure.toString()})(${JSON.stringify({count,style:c.style,warmup:report.warmup,samples:report.samples,representation:c.representation,distanceScale:c.distanceScale})})`);report.runs.push({round,label:c.label,...result});fs.writeFileSync(path.join(out,'raw.json'),JSON.stringify(report,null,2));console.log(round,count,c.label);}
        }
        assert.deepEqual(b.errors,[]);report.errors=b.errors;
        const quantile=(a,p)=>{a=[...a].sort((x,y)=>x-y);return a[Math.min(a.length-1,Math.floor(a.length*p))];};
        report.summary=[];
        for(const count of [40000,307200])for(const label of [...new Set(report.runs.map(r=>r.label))]){const runs=report.runs.filter(r=>r.count===count&&r.label===label),rows=runs.flatMap(r=>r.rows),timings={};for(const key of Object.keys(rows[0]).filter(k=>k.endsWith('Ms'))){const values=rows.map(r=>r[key]);timings[key]={median:quantile(values,.5),p95:quantile(values,.95),roundMedians:runs.map(r=>quantile(r.rows.map(x=>x[key]),.5))};}report.summary.push({count,label,capacity:runs[0].counters.bufferBytes,drawingBuffer:runs[0].counters.drawingBuffer,initialUploads:runs[0].initialUploads,perIterationUploads:rows[0].uploads,timings});}
        fs.writeFileSync(path.join(out,'report.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report.summary,null,2));
    }finally{await b.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
