/* Resource accounting across real console transitions. Loopback/disposable Chromium only. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
function instrumentation(){
    const report=window.lifecycleProbe={contexts:[],listeners:{canvas:0,document:0,window:0},raf:new Set(),maxRAF:0,observers:0};
    const listeners=new WeakMap(),add=EventTarget.prototype.addEventListener,remove=EventTarget.prototype.removeEventListener;
    const kind=t=>t instanceof HTMLCanvasElement?'canvas':t===document?'document':t===window?'window':null;
    EventTarget.prototype.addEventListener=function(type,fn,options){const k=kind(this);if(k&&fn){let m=listeners.get(this);if(!m)listeners.set(this,m=[]);const capture=typeof options==='boolean'?options:!!options?.capture;if(!m.some(v=>v.type===type&&v.fn===fn&&v.capture===capture)){m.push({type,fn,capture});report.listeners[k]++;}}return add.call(this,type,fn,options);};
    EventTarget.prototype.removeEventListener=function(type,fn,options){const k=kind(this),m=listeners.get(this),capture=typeof options==='boolean'?options:!!options?.capture;if(k&&m){const i=m.findIndex(v=>v.type===type&&v.fn===fn&&v.capture===capture);if(i>=0){m.splice(i,1);report.listeners[k]--;}}return remove.call(this,type,fn,options);};
    const request=window.requestAnimationFrame,cancel=window.cancelAnimationFrame;
    window.requestAnimationFrame=fn=>{let id=request.call(window,t=>{report.raf.delete(id);fn(t);});report.raf.add(id);report.maxRAF=Math.max(report.maxRAF,report.raf.size);return id;};
    window.cancelAnimationFrame=id=>{report.raf.delete(id);return cancel.call(window,id);};
    const Observer=ResizeObserver;window.ResizeObserver=class extends Observer{constructor(fn){super(fn);this.probeObserved=false;}observe(...a){if(!this.probeObserved){this.probeObserved=true;report.observers++;}return super.observe(...a);}disconnect(){if(this.probeObserved){this.probeObserved=false;report.observers--;}return super.disconnect();}};
    const get=HTMLCanvasElement.prototype.getContext,seen=new WeakSet();
    HTMLCanvasElement.prototype.getContext=function(type,...args){const gl=get.call(this,type,...args);if(gl&&type.includes('webgl')&&!seen.has(gl)){
        seen.add(gl);const record={lost:0,created:{},deleted:{},live:{}};report.contexts.push(record);
        for(const resource of ['Buffer','Program','Shader','VertexArray','Texture','Framebuffer','Renderbuffer']){
            const create='create'+resource,del='delete'+resource;if(!gl[create])continue;const originalCreate=gl[create].bind(gl),originalDelete=gl[del].bind(gl),live=new Set();record.created[resource]=record.deleted[resource]=record.live[resource]=0;
            gl[create]=(...a)=>{const object=originalCreate(...a);if(object){live.add(object);record.created[resource]++;record.live[resource]=live.size;}return object;};
            gl[del]=object=>{if(live.delete(object)){record.deleted[resource]++;record.live[resource]=live.size;}return originalDelete(object);};
        }
        const extension=gl.getExtension.bind(gl);let wrapped=false;gl.getExtension=name=>{const value=extension(name);if(name==='WEBGL_lose_context'&&value&&!wrapped){wrapped=true;const lose=value.loseContext.bind(value);value.loseContext=()=>{record.lost++;return lose();};}return value;};
    }return gl;};
    window.lifecycleSnapshot=()=>({contexts:JSON.parse(JSON.stringify(report.contexts)),listeners:{...report.listeners},raf:report.raf.size,maxRAF:report.maxRAF,observers:report.observers,heap:performance.memory?.usedJSHeapSize??null});
}
(async()=>{
    const mock=await fixture(),b=await browserFor(mock.origin),out=path.resolve('_build/pointcloud-renderer-style/validation/lifecycle');fs.mkdirSync(out,{recursive:true});
    try{
        await b.command('Page.addScriptToEvaluateOnNewDocument',{source:`(${instrumentation.toString()})()`});await b.viewport(1280,720);
        await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.wait('ChitonConsoleApp.view.cloud?.count===307200');
        await b.evaluate('ChitonConsoleApp.stop()');await b.sleep(100);const snapshots=[],heapSamples=[];
        async function heapSample(cycle){await b.command('HeapProfiler.collectGarbage');heapSamples.push({cycle,...await b.command('Runtime.getHeapUsage')});}
        await heapSample(-1);
        for(let cycle=0;cycle<50;cycle++){
            const snapshot=await b.evaluate(`(async()=>{const a=ChitonConsoleApp;a.source.mode=1;a.applyStatus(await a.source.readStatus());a.source.resume();a.start();a.source.mode=4;a.applyStatus(await a.source.readStatus());a.stop();const v=a.view.cloud;v.setActive(true);v.update({source:'demo',revision:1,frameType:'depth',units:'depth_sensor_units',unitsScope:'full_sensor',recordCount:2,records:new Int16Array([0,0,0,500,50,0,0,750])},{min:0,max:1000});v.render();const before=v.counters(),programs=v.renderer.info.programs.length,measurement=JSON.stringify({latest:a.state.latest,history:a.state.history,filter:a.state.filter,kalman:a.state.kalman,estimate:a.state.estimate});for(let i=0;i<30;i++)v.setPointStyle({shape:i%2?'circle':'square',diameterCssPx:1+(i%19)*.5,sizing:i%2?'screen':'perspective'});v.render();if(measurement!==JSON.stringify({latest:a.state.latest,history:a.state.history,filter:a.state.filter,kalman:a.state.kalman,estimate:a.state.estimate}))throw new Error('Appearance changed measurement/history/filter');if(v.renderer.info.programs.length!==programs||v.uploads!==before.uploads)throw new Error('Style resource/upload growth');v.setActive(false);return {cycle:${cycle},probe:lifecycleSnapshot(),counters:v.counters(),programs};})()`);
            snapshots.push(snapshot);assert.equal(snapshot.programs,2);assert.equal(snapshot.counters.geometries,2);assert.equal(snapshot.counters.raf,false);assert.equal(snapshot.counters.bufferBytes,2457600);
            const contexts=snapshot.probe.contexts;assert.equal(contexts.length,cycle+2);assert.equal(contexts.filter(c=>!c.lost).length,1);
            // r128's two internal fallback textures have no explicit delete; forced context loss reclaims them.
            for(const retired of contexts.slice(0,-1))for(const [kind,n] of Object.entries(retired.live))assert.equal(n,kind==='Texture'?2:0,`cycle ${cycle}: retired ${kind}`);
            const current=contexts.at(-1);assert.equal(current.live.Program,2);assert.equal(current.live.Buffer,3);assert.equal(current.live.Shader,0);
            if(cycle)assert.deepEqual(snapshot.probe.listeners,snapshots[0].probe.listeners);assert.equal(snapshot.probe.observers,snapshots[0].probe.observers);assert(snapshot.probe.raf<=2);assert(snapshot.probe.maxRAF<=3);
            if(cycle%10===9)await heapSample(cycle);
        }
        // After warm-up, allow small browser/probe bookkeeping, not another full-capacity attribute buffer.
        for(const sample of heapSamples.slice(1)){assert(sample.backingStorageSize<=heapSamples[1].backingStorageSize+65536);assert(sample.usedSize<=heapSamples[1].usedSize+1048576);assert(sample.embedderHeapUsedSize<=heapSamples[1].embedderHeapUsedSize+1048576);}
        // Idle/hide/pause cycles must have zero renderer RAF and no new contexts/resources/listeners.
        const idle=await b.evaluate(`(async()=>{const a=ChitonConsoleApp;for(let i=0;i<50;i++){Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'));if(a.cloud.running||a.view.cloud.raf!=null)throw new Error('Hidden work');Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'));a.stop();}await new Promise(r=>setTimeout(r,100));return lifecycleSnapshot();})()`);
        assert.equal(idle.contexts.length,51);assert.deepEqual(idle.listeners,snapshots[0].probe.listeners);assert.equal(idle.raf,0);assert.equal(await b.evaluate('document.querySelectorAll("#cloud-canvas").length'),1);
        const disposed=await b.evaluate(`ChitonConsoleApp.view.cloud.dispose();lifecycleSnapshot()`);for(const c of disposed.contexts){assert.equal(c.lost,1);for(const [kind,n] of Object.entries(c.live))assert.equal(n,kind==='Texture'?2:0);}assert.equal(disposed.observers,idle.observers-1);assert(disposed.listeners.canvas<idle.listeners.canvas);assert.equal(disposed.raf,0);assert.deepEqual(b.errors,[]);
        fs.writeFileSync(path.join(out,'report.json'),JSON.stringify({passed:true,snapshots,heapSamples,idle,disposed,errors:b.errors,limitations:'Heap samples use test-only CDP garbage collection and Runtime.getHeapUsage; performance.memory snapshots are unsynchronized trend only. Neither is an exact browser/GPU memory claim. Listener accounting covers canvas/document/window, excludes ordinary controls. Two internal r128 fallback textures per context lack explicit delete calls but are reclaimed by the observed forced context loss; all other tracked GL resources explicitly delete. RAF counts include page view work, not only renderer.'},null,2));
        console.log('console_cloud_lifecycle_browser: 50 real context transitions + 1500 style sets + 50 hidden cycles; balanced GL buffers/programs/shaders, bounded listeners/observers/RAF, disposed contexts passed');
    }finally{await b.close();await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
