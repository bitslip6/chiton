/* Real loopback Chromium EDL/framebuffer/UI/lifecycle regression; no hardware. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
async function gpuChecks(){
    const C=ChitonConsole,T=THREE,host=document.createElement('div');host.style.cssText='position:fixed;width:256px;height:256px';document.body.append(host);
    const v=new C.CloudView(host),gl=v.renderer.getContext();if(v.raf!==null)cancelAnimationFrame(v.raf);v.raf=null;v.dirty=()=>{};v.axes.visible=false;
    const check=(value,message)=>{if(!value)throw new Error(message);},equal=(a,b,m)=>check(JSON.stringify(a)===JSON.stringify(b),m);
    const snapshot=records=>({source:'test',revision:1,frameType:'depth',units:'mm',unitsScope:'calibration_aoi',frameId:77,records:new Int16Array(records),recordCount:records.length/4});
    const records=[];for(let y=-100;y<=100;y+=3)for(let x=-100;x<=100;x+=3)records.push(x,y,x<0?0:-80,1000);
    const s=snapshot(records);v.update(s,null);v.setPointStyle({shape:'square',diameterCssPx:6,sizing:'screen'});
    v.nav.target=[0,0,0];v.nav.radius=500;v.nav.base=500;v.nav.theta=0;v.nav.phi=Math.PI/2;
    const output=new T.WebGLRenderTarget(256,256,{minFilter:T.NearestFilter,magFilter:T.NearestFilter});
    const pixels=()=>{v.renderer.setRenderTarget(output);v.render();const a=new Uint8Array(256*256*4);v.renderer.readRenderTargetPixels(output,0,0,256,256,a);return a;};
    const plain=pixels(),before=v.counters(),pose=v.nav.pose(),version=v.attributeBuffer.version;
    check(before.edlSupported,'test WebGL path supports optional EDL');v.setEyeDomeLighting(true);const shaded=pixels();
    check(v.eyeDomeLighting&&!v.edlError,'EDL did not silently fall back: '+v.edlError);
    let darker=0,unchanged=0,background=0;for(let i=0;i<plain.length;i+=4){
        check(shaded[i]<=plain[i]+1&&shaded[i+1]<=plain[i+1]+1&&shaded[i+2]<=plain[i+2]+1,'EDL only shades RGB');
        if(plain[i]>30){if(shaded[i]<plain[i]-3)darker++;else if(Math.abs(shaded[i]-plain[i])<=1)unchanged++;}
        else {check(Math.abs(shaded[i]-plain[i])<=1,'background unchanged');background++;}
    }
    check(darker>100&&unchanged>100&&background>100,'depth edges shaded, flat interiors/clear remain: '+JSON.stringify({darker,unchanged,background}));
    equal(v.nav.pose(),pose,'lighting preserves camera');check(v.uploads===before.uploads&&v.attributeBuffer.version===version,'lighting no point uploads');equal([...s.records],records,'source preserved');
    const allocations={geometries:v.renderer.info.memory.geometries,textures:v.renderer.info.memory.textures,programs:v.renderer.info.programs.length};
    for(let i=0;i<15;i++){v.setEyeDomeLighting(false);pixels();v.setEyeDomeLighting(true);pixels();}
    equal({geometries:v.renderer.info.memory.geometries,textures:v.renderer.info.memory.textures,programs:v.renderer.info.programs.length},allocations,'EDL resources stable');
    v.setEyeDomeLighting(false);equal([...pixels()],[...plain],'plain mode returns exact pixels');
    v.setProjection('orthographic');v.setEyeDomeLighting(true);pixels();check(!v.edlError&&gl.getError()===gl.NO_ERROR,'orthographic EDL');
    host.style.width='300px';v.resize();pixels();equal(v.edl.target.width,256,'output-target EDL size');
    v.renderer.setRenderTarget(null);v.render();equal([v.edl.target.width,v.edl.target.height],[v.canvas.width,v.canvas.height],'resized default EDL target');
    const fill=CanvasRenderingContext2D.prototype.fillText,text=[];CanvasRenderingContext2D.prototype.fillText=function(...a){text.push(a[0]);return fill.apply(this,a);};
    let blob;try{blob=await v.png(s,{source:'demo',overlays:false});}finally{CanvasRenderingContext2D.prototype.fillText=fill;}
    check(text.some(t=>t.includes('EDL on'))&&text.some(t=>t.includes('orthographic')),'PNG lighting/projection provenance');
    const image=await createImageBitmap(blob);check(image.width===v.canvas.width,'actual shaded PNG');image.close();
    // Robust main fit only changes framing; all signed records remain represented and uploaded once.
    const outliers=[];for(let i=0;i<1000;i++)outliers.push(i%10,i%9,i%7,1000);outliers.push(32767,-32768,32767,-32768);
    v.update(snapshot(outliers),null);const count=v.count,uploads=v.uploads;v.fit('all');const all=v.nav.radius;v.fit('main');check(v.nav.radius<all/10,'outlier framing improved');
    check(v.count===count&&v.uploads===uploads&&v.geometry.drawRange.count===count,'main fit never filters/uploads');check(v.framing.included===1000,'reported main frame subset');
    const panStart=[...v.nav.target];v.canvas.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',shiftKey:true,cancelable:true}));
    const panMove=Math.hypot(...v.nav.target.map((x,i)=>x-panStart[i]));check(panMove>0&&panMove<v.nav.base*.1,'keyboard pan after main fit remains continuous');
    const mainPose=v.nav.pose();v.setColorRange({min:600,max:5600});v.setPointStyle({shape:'circle',diameterCssPx:4,sizing:'screen'});equal(v.nav.pose(),mainPose,'style/range no refit');
    v.setActive(false);const paints=v.paints.length;v.setEyeDomeLighting(false);v.fit('all');v.setOrientation('front');v.setProjection('perspective');check(v.raf===null&&v.paints.length===paints,'paused no paint');
    const images={};for(const [name,p] of [['plain',plain],['edl',shaded]]){const c=document.createElement('canvas');c.width=c.height=256;c.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(p),256,256),0,0);images[name]=c.toDataURL('image/png').split(',')[1];}
    const result={webgl:gl.getParameter(gl.VERSION),darker,unchanged,background,allocations,pngText:text,framing:v.framing,images};
    const status=gl.checkFramebufferStatus.bind(gl);gl.checkFramebufferStatus=()=>gl.FRAMEBUFFER_UNSUPPORTED;
    v.setActive(true);v.setEyeDomeLighting(true);pixels();gl.checkFramebufferStatus=status;
    check(!v.eyeDomeLighting&&!!v.edlError&&v.edl===null&&!v.failed,'EDL failure falls back to available plain points');
    output.dispose();v.dispose();check(v.edl===null&&host.children.length===0,'EDL disposed');host.remove();return result;
}
(async()=>{
    const mock=await fixture(),b=await browserFor(mock.origin),out=path.resolve('_build/pointcloud-framing-edl');fs.mkdirSync(path.join(out,'validation'),{recursive:true});fs.mkdirSync(path.join(out,'screenshots'),{recursive:true});
    try{
        const reports=[];
        for(const webgl1 of [false,true]){
            let injection;if(webgl1)injection=await b.command('Page.addScriptToEvaluateOnNewDocument',{source:`const original=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(type,...args){return type==='webgl2'?null:original.call(this,type,...args);};`});
            await b.viewport(1280,720,1);await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.evaluate('ChitonConsoleApp.prepareCapture({cloudMode:true})');
            const report=await b.evaluate(`(${gpuChecks.toString()})()`);for(const [name,bytes] of Object.entries(report.images))fs.writeFileSync(path.join(out,'screenshots',`${webgl1?'webgl1':'webgl2'}-${name}-depth-step.png`),Buffer.from(bytes,'base64'));delete report.images;reports.push(report);if(injection)await b.command('Page.removeScriptToEvaluateOnNewDocument',{identifier:injection.identifier});
        }
        await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.evaluate('ChitonConsoleApp.prepareCapture({cloudMode:true})');
        const click=id=>b.evaluate(`document.getElementById(${JSON.stringify(id)}).click()`,true);
        await b.screenshot(path.join(out,'screenshots/edl-off.png'));
        await click('cloud-edl');await b.wait('ChitonConsoleApp.view.cloud.edl!==null');await b.screenshot(path.join(out,'screenshots/edl-on.png'));
        await click('cloud-fit-main');assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.framing.mode'),'main');
        await b.evaluate(`document.getElementById('cloud-orientation').value='top';document.getElementById('cloud-orientation').dispatchEvent(new Event('change'));document.getElementById('cloud-projection').value='orthographic';document.getElementById('cloud-projection').dispatchEvent(new Event('change'));`);
        const state=await b.evaluate(`JSON.stringify({latest:ChitonConsoleApp.state.latest,history:ChitonConsoleApp.state.history,kalman:ChitonConsoleApp.state.kalman})`);
        await click('cloud-fit-all');assert.equal(await b.evaluate(`JSON.stringify({latest:ChitonConsoleApp.state.latest,history:ChitonConsoleApp.state.history,kalman:ChitonConsoleApp.state.kalman})`),state);
        // Recreation preserves local lighting/projection without writing settings.
        await b.evaluate(`(async()=>{const a=ChitonConsoleApp;a.source.mode=1;a.applyStatus(await a.source.readStatus());a.source.mode=4;a.applyStatus(await a.source.readStatus());await a.prepareCapture({cloudMode:true});})()`);
        assert.deepEqual(await b.evaluate(`({edl:ChitonConsoleApp.view.cloud.eyeDomeLighting,projection:ChitonConsoleApp.view.cloud.projection})`),{edl:true,projection:'orthographic'});
        await b.evaluate(`ChitonConsoleApp.view.cloud.setOrientation('top');ChitonConsoleApp.view.cloud.render()`);
        assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.nav.phi'),.0001);
        await b.screenshot(path.join(out,'screenshots/orthographic-top-edl.png'));
        for(const [width,height] of [[1920,1080],[1280,720],[390,844],[320,844],[640,360]]){await b.viewport(width,height,1);await b.sleep(100);assert.equal(await b.evaluate('document.documentElement.scrollWidth<=innerWidth'),true);}
        await b.screenshot(path.join(out,'screenshots/mobile-controls.png'));
        await b.viewport(320,844,1);await click('fullscreen');await b.wait(`document.fullscreenElement?.id==='viewport' && document.getElementById('viewport').contains(document.getElementById('cloud-edl'))`);
        assert.equal(await b.evaluate(`document.getElementById('viewport').contains(document.getElementById('cloud-edl'))`),true);
        await b.evaluate(`document.getElementById('cloud-edl').scrollIntoView({block:'center'})`);await click('cloud-edl');
        assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.eyeDomeLighting'),false);await click('cloud-edl');
        await b.screenshot(path.join(out,'screenshots/fullscreen-mobile.png'));await b.evaluate('document.exitFullscreen()',true);
        await b.wait(`document.getElementById('cloud-navigation').parentElement.id==='display-navigation'`);
        await b.evaluate(`ChitonConsoleApp.source.resume();ChitonConsoleApp.start()`);await b.wait('ChitonConsoleApp.view.cloud?.count>0');
        await b.evaluate('ChitonConsoleApp.view.cloud.renderer.forceContextLoss()');await b.wait('ChitonConsoleApp.state.cloudUnavailable!==null');
        await click('cloud-retry');await b.wait('ChitonConsoleApp.view.cloud?.count>0 && ChitonConsoleApp.view.cloud.eyeDomeLighting');
        assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.projection'),'orthographic');
        // Real live fixture, with no device writes from any rendering/framing control.
        mock.source.mode=4;mock.source.seek(1000);await b.navigate(mock.origin+'/console.html?source=live');await b.wait('ChitonConsoleApp.view.cloud?.count>0');
        await click('cloud-edl');await click('cloud-fit-main');await click('cloud-fit-all');
        await click('pause-viewer');await b.sleep(100);
        const inactive=await b.evaluate(`({last:ChitonConsoleApp.view.cloud.lastPaint,uploads:ChitonConsoleApp.view.cloud.uploads,requests:${mock.requests.length}})`),requests=mock.requests.length;
        await click('cloud-edl');await click('cloud-fit-main');await b.sleep(150);
        assert.deepEqual(await b.evaluate(`({last:ChitonConsoleApp.view.cloud.lastPaint,uploads:ChitonConsoleApp.view.cloud.uploads,requests:${requests}})`),inactive);
        assert.equal(mock.requests.length,requests);assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.raf'),null);assert.equal(mock.source.mutations.length,0);
        // Unsupported optional extension path must retain the basic viewer, not fail it.
        await b.command('Page.addScriptToEvaluateOnNewDocument',{source:`const old=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(t,...args){if(t==='webgl2')return null;const g=old.call(this,t,...args);if(g&&t.includes('webgl')){const e=g.getExtension.bind(g);g.getExtension=n=>n==='WEBGL_depth_texture'?null:e(n);}return g;};`});
        await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.evaluate('ChitonConsoleApp.prepareCapture({cloudMode:true,eyeDomeLighting:true})');
        assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.count'),307200);assert.equal(await b.evaluate('document.getElementById("cloud-edl").disabled'),true);assert.equal(await b.evaluate('ChitonConsoleApp.view.cloud.eyeDomeLighting'),false);
        assert.deepEqual(b.errors,[]);fs.writeFileSync(path.join(out,'validation/report.json'),JSON.stringify({reports,errors:b.errors,requests:mock.requests},null,2));
        console.log('console_cloud_framing_edl_browser: actual WebGL1/2 depth shading, plain parity, camera-only robust fit, orthographic/presets, PNG, resource reuse/disposal, local state and optional unsupported path passed');
    }finally{await b.close();await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
