#!/usr/bin/env node
/* Loopback synthetic data only. Synchronous readback is test-only, never added to runtime. */
'use strict';
const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {fixture}=require('../tests/console_fixture'),{browserFor}=require('./console-browser-driver');
async function benchmark(){
    const C=ChitonConsole,host=document.createElement('div');host.style.cssText='position:fixed;width:800px;height:600px';document.body.append(host);
    const v=new C.CloudView(host);if(v.raf!==null)cancelAnimationFrame(v.raf);v.raf=null;v.dirty=()=>{};v.axes.visible=false;
    const gl=v.renderer.getContext(),read=new Uint8Array(4),complete=()=>{gl.finish();gl.readPixels(400,300,1,1,gl.RGBA,gl.UNSIGNED_BYTE,read);};
    const runs=[],warmup=6,samples=15;
    for(const count of [40000,307200]){
        const records=new Int16Array(count*4);for(let i=0;i<count;i++){const x=i%640,y=Math.floor(i/640);records.set([(x-320)*6,(y-240)*6,((x*17+y*11)%1000)-500,600+((i*37)%2600)],i*4);}
        const s={source:'benchmark',revision:1,frameType:'depth',units:'mm',unitsScope:'calibration_aoi',records,recordCount:count,frameId:1};
        v.update(s,{min:600,max:5600});v.nav.target=[0,0,0];v.nav.radius=v.nav.base=7000;v.nav.theta=.35;v.nav.phi=1.25;v.setPointStyle({shape:'circle',diameterCssPx:2.5,sizing:'screen'});
        for(let round=0;round<2;round++)for(const enabled of round?[true,false]:[false,true]){
            v.setEyeDomeLighting(enabled);v.render();complete();if(enabled&&!v.eyeDomeLighting)throw new Error('EDL unsupported: '+v.edlError);
            const rows=[];
            for(let i=0;i<warmup+samples;i++){
                const a=performance.now();v.update(s,{min:600,max:5600});const b=performance.now();v.render();complete();const c=performance.now();
                v.nav.orbit(i%2?.001:-.001,0);const d=performance.now();v.render();complete();const e=performance.now();
                if(i>=warmup)rows.push({prepare:b-a,updatedDraw:c-b,orbit:e-d});
            }
            runs.push({count,round,enabled,rows,counters:v.counters()});
        }
    }
    const environment={webgl:gl.getParameter(gl.VERSION),renderer:gl.getParameter(gl.getExtension('WEBGL_debug_renderer_info').UNMASKED_RENDERER_WEBGL),dpr:v.effectivePixelRatio,buffer:[v.canvas.width,v.canvas.height]};
    v.dispose();host.remove();return {runs,warmup,samples,environment};
}
(async()=>{
    const out=path.resolve(process.argv[2]||'_build/pointcloud-framing-edl/validation/benchmark.json');fs.mkdirSync(path.dirname(out),{recursive:true});
    const mock=await fixture(),b=await browserFor(mock.origin);
    try{
        await b.viewport(800,600,1);await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.evaluate('ChitonConsoleApp.stop()');
        const result=await b.evaluate(`(${benchmark.toString()})()`),q=(a,p)=>[...a].sort((x,y)=>x-y)[Math.min(a.length-1,Math.floor(a.length*p))];result.summary=[];
        for(const count of [40000,307200])for(const enabled of [false,true]){
            const rows=result.runs.filter(r=>r.count===count&&r.enabled===enabled).flatMap(r=>r.rows),timings={};
            for(const key of ['prepare','updatedDraw','orbit'])timings[key]={median:q(rows.map(r=>r[key]),.5),p95:q(rows.map(r=>r[key]),.95)};
            result.summary.push({count,enabled,timings});
        }
        Object.assign(result,{browser:await b.command('Browser.getVersion',{},null),cpu:os.cpus()[0]?.model,node:process.version,notes:'800x600 DPR1 circle2.5, fixed synthetic input/pose/range, two reversed-order rounds; test-only finish+readback completion. EDL adds a color/depth target and fullscreen pass. Software WebGL does not predict physical GPU timing.'});
        fs.writeFileSync(out,JSON.stringify(result,null,2));console.log(JSON.stringify(result.summary,null,2));if(b.errors.length)throw new Error(JSON.stringify(b.errors));
    }finally{await b.close();await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
