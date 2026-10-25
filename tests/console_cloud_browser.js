/* Actual WebGL, input, native PNG and transport lifecycle; loopback only. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
(async()=>{
    const mock=await fixture(),b=await browserFor(mock.origin),out=path.resolve('_build/console-review/3d/browser');fs.mkdirSync(out,{recursive:true});
    const a='ChitonConsoleApp',click=id=>b.evaluate(`document.getElementById(${JSON.stringify(id)}).click()`,true),pose=()=>b.evaluate(`${a}.view.cloud.nav.pose()`);
    const paint=()=>b.evaluate(`new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(()=>requestAnimationFrame(r))))`);
    async function downloaded(id='png'){const before=b.downloads.filter(e=>e.method==='Browser.downloadWillBegin').length;
        if(id==='obj')assert.equal(await b.evaluate(`(()=>{const button=document.getElementById('obj');button.click();button.click();return ${a}.state.objPending&&button.disabled&&button.getAttribute('aria-busy')==='true';})()`,true),true);
        else await click(id);
        for(let i=0;i<150;i++){const starts=b.downloads.filter(e=>e.method==='Browser.downloadWillBegin'),e=starts[before];if(e&&b.downloads.some(v=>v.method==='Browser.downloadProgress'&&v.params.guid===e.params.guid&&v.params.state==='completed')){assert.equal(starts.length,before+1);if(id==='obj')assert.equal(e.params.suggestedFilename,'chiton-pointcloud.obj');return fs.readFileSync(path.join(out,e.params.guid));}await b.sleep(30);}throw new Error('Download timeout: '+id);}
    async function pointPixels(bytes){return b.evaluate(`(async()=>{const bytes=Uint8Array.from(atob('${bytes.toString('base64')}'),c=>c.charCodeAt(0)),image=await createImageBitmap(new Blob([bytes],{type:'image/png'}));const c=document.createElement('canvas');c.width=image.width;c.height=image.height;const ctx=c.getContext('2d');ctx.drawImage(image,0,0);image.close();const p=ctx.getImageData(0,30,c.width,Math.max(1,c.height-150)).data;let colored=0;for(let i=0;i<p.length;i+=4)if(Math.max(p[i],p[i+1],p[i+2])-Math.min(p[i],p[i+1],p[i+2])>50&&Math.max(p[i],p[i+1],p[i+2])>80)colored++;return {colored,width:c.width,height:c.height};})()`);}
    try{
        await b.command('Browser.setDownloadBehavior',{behavior:'allowAndName',downloadPath:out,eventsEnabled:true},null);
        await b.viewport(1280,720);await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.wait(`${a}.view.cloud?.count===307200`);
        await b.evaluate(`${a}.prepareCapture({cloudMode:true,filter:true})`);await paint();
        assert.equal(mock.requests.some(p=>!/^\/[\w.-]+\.(html|js|css|png)$/.test(p)),false);assert.equal(mock.clients.size,0);
        assert.equal(await b.evaluate('document.getElementById("obj").closest("#display-export")!=null && document.getElementById("obj").disabled'),true);
        const demoDownloads=b.downloads.length;await click('obj');assert.equal(b.downloads.length,demoDownloads);
        assert.equal(await b.evaluate(`${a}.state.latest.statisticsKind`),'temporal_observed_frames');assert.equal(await b.evaluate('document.getElementById("sensor").hidden'),true);
        const initial=await pose();const r=await b.evaluate(`(()=>{const r=document.getElementById('cloud-canvas').getBoundingClientRect();return {x:r.x+r.width*.6,y:r.y+r.height*.5};})()`);
        await b.command('Input.dispatchMouseEvent',{type:'mousePressed',...r,button:'left',clickCount:1});await b.command('Input.dispatchMouseEvent',{type:'mouseMoved',x:r.x+65,y:r.y+30,button:'left',buttons:1});await b.command('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x+65,y:r.y+30,button:'left',clickCount:1});
        assert.notDeepEqual(await pose(),initial);assert.equal(await b.evaluate(`${a}.view.cloud.pointers.size`),0);
        const selection=await b.evaluate(`${a}.state.selection`);await b.evaluate('document.getElementById("cloud-canvas").focus()');
        await b.command('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});assert.deepEqual(await b.evaluate(`${a}.state.selection`),selection);
        const p1=await pose();await b.command('Input.dispatchMouseEvent',{type:'mouseWheel',...r,deltaY:80,deltaX:0});assert.notEqual((await pose()).radius,p1.radius);
        await b.evaluate(`${a}.view.cloud.mode='pan'`);const beforePan=await pose();await b.command('Input.dispatchMouseEvent',{type:'mousePressed',...r,button:'right',clickCount:1});await b.command('Input.dispatchMouseEvent',{type:'mouseMoved',x:r.x+30,y:r.y-20,button:'right',buttons:2});await b.command('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x+30,y:r.y-20,button:'right',clickCount:1});assert.notDeepEqual((await pose()).target,beforePan.target);
        const stable=await pose();await b.evaluate(`(async()=>{const a=${a};a.source.seek(6500);const m=await a.source.readCloud();a.view.cloud.update(m,a.state.device.range);})()`);assert.deepEqual(await pose(),stable);
        await b.viewport(1024,768,2);await paint();assert.deepEqual(await pose(),stable);assert((await b.evaluate(`${a}.view.cloud.canvas.width`))<=4096);
        assert.equal(await b.evaluate('document.getElementById("fit")'),null);
        await b.evaluate('document.getElementById("cloud-canvas").focus()');await b.command('Input.dispatchKeyEvent',{type:'keyDown',key:'f',code:'KeyF',windowsVirtualKeyCode:70});
        assert.notDeepEqual((await pose()).target,stable.target);await click('reset-cloud');const reset=await pose();assert.equal(reset.theta,.35);assert.equal(reset.phi,1.25);
        await click('fullscreen');await b.wait('document.fullscreenElement?.id==="viewport"');await paint();assert.deepEqual(await pose(),reset);
        await b.evaluate('document.exitFullscreen()',true);await paint();assert.deepEqual(await pose(),reset);
        // Real multi-touch pan/pinch followed by touchCancel, no stuck pointers.
        await b.command('Emulation.setTouchEmulationEnabled',{enabled:true,maxTouchPoints:2});
        const touch=await b.evaluate(`(()=>{const r=document.getElementById('cloud-canvas').getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
        await b.command('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{...touch,id:1},{x:touch.x+60,y:touch.y,id:2}]});
        await b.command('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:touch.x-20,y:touch.y+20,id:1},{x:touch.x+100,y:touch.y+20,id:2}]});
        await b.command('Input.dispatchTouchEvent',{type:'touchCancel',touchPoints:[]});assert.equal(await b.evaluate(`${a}.view.cloud.pointers.size`),0);assert.notDeepEqual(await pose(),reset);
        await b.command('Emulation.setTouchEmulationEnabled',{enabled:false});
        await b.evaluate('document.getElementById("png-overlays").checked=false');const png=await downloaded(),pixels=await pointPixels(png);assert(pixels.colored>1000,JSON.stringify(pixels));
        await click('reset-cloud');const png2=await downloaded();assert.notEqual(crypto.createHash('sha256').update(png).digest('hex'),crypto.createHash('sha256').update(png2).digest('hex'));
        fs.writeFileSync(path.join(out,'cloud-pose.png'),png);fs.writeFileSync(path.join(out,'cloud-reset.png'),png2);await b.screenshot(path.join(out,'demo-desktop.png'));
        // Numeric-only ROI conversion and preserved independent point estimator.
        await b.evaluate(`(async()=>{const a=${a};a.source.mode=1;a.applyStatus(await a.source.readStatus());a.setSelection({kind:'roi',x:400,y:200,x2:500,y2:300});a.source.mode=4;a.applyStatus(await a.source.readStatus());})()`);
        assert.deepEqual(await b.evaluate(`${a}.state.selection`),{kind:'point',x:450,y:250});assert.equal(await b.evaluate('document.getElementById("roi-mode").disabled'),true);
        // Fifty transitions dispose contexts/objects, no old canvases or work chains.
        await b.evaluate(`(async()=>{const a=${a};a.source.resume();a.start();for(let i=0;i<50;i++){a.source.mode=1;a.applyStatus(await a.source.readStatus());if(a.view.cloud||a.cloud.running)throw new Error('Cloud retained in JPEG');a.source.mode=4;a.applyStatus(await a.source.readStatus());if(document.querySelectorAll('#cloud-canvas').length!==1)throw new Error('Renderer accumulated');await new Promise(r=>setTimeout(r,10));}a.stop();})()`);
        assert.equal(await b.evaluate(`${a}.cloud.running`),false);assert.equal(await b.evaluate(`${a}.view.cloud.raf`),null);
        await b.evaluate(`(async()=>{const a=${a};for(let i=0;i<50;i++){Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'));if(a.cloud.running||a.view.cloud.raf!=null)throw new Error('Hidden cloud work');Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'));await new Promise(r=>setTimeout(r,5));}a.stop();})()`);
        assert.equal(await b.evaluate('document.querySelectorAll("#cloud-canvas").length'),1);
        // Explicit context-loss and retry, no cloud fetch while unavailable.
        await b.evaluate(`${a}.start()`);await b.wait(`${a}.state.cloud.latest!=null`);
        await b.evaluate(`${a}.view.cloud.renderer.getContext().getExtension('WEBGL_lose_context').loseContext()`);await b.wait(`${a}.state.cloudUnavailable?.includes('context lost')`);assert.equal(await b.evaluate(`${a}.cloud.running`),false);
        await click('cloud-retry');await b.wait(`${a}.view.cloud?.count===307200`);
        // Missing WebGL fallback on a bounded explicit retry, then restore creation.
        await b.evaluate(`window.savedGetContext=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(type,...args){if(type.includes('webgl'))return null;return savedGetContext.call(this,type,...args);};document.getElementById('cloud-retry').click()`);
        await b.wait(`${a}.state.cloudUnavailable?.includes('WebGL unavailable')`);assert.equal(await b.evaluate(`${a}.cloud.running`),false);
        await b.evaluate('HTMLCanvasElement.prototype.getContext=savedGetContext');await click('cloud-retry');await b.wait(`${a}.view.cloud?.count===307200`);
        // Live fixture: binary mode only, separate measurements, duplicates/stale/errors/empty/IR.
        mock.source.mode=4;mock.source.seek(1000);mock.requests.length=0;await b.navigate(mock.origin+'/console.html?source=live');await b.wait(`${a}.state.cloud.latest?.frameId===31 && ${a}.state.latest!=null`);
        assert.equal(mock.clients.size,0);assert.equal(mock.requests.includes('/image/camera'),false);assert.equal(mock.source.mutations.length,0);assert.equal(mock.maximum.cloud,1);
        // Visible device export: one bounded request even for rapid double-clicks, no view transform/filtering.
        assert.equal(await b.evaluate('document.getElementById("obj").disabled'),false);
        const expected=mock.source.cloudSnapshot(),beforeOBJ=mock.counts['/pointcloud_obj/camera']||0;
        const obj=await downloaded('obj'),vertices=obj.toString().trim().split('\n');assert.equal(vertices.length,expected.recordCount);
        for(const i of [0,12345,expected.recordCount-1]){const fields=vertices[i].split(' ');assert.equal(fields[0],'v');fields.slice(1).forEach((n,k)=>assert(Math.abs(Number(n)-expected.records[i*4+k]/6000)<.000051));}
        assert.equal(mock.counts['/pointcloud_obj/camera'],beforeOBJ+1);
        const raw=()=>b.evaluate(`(async()=>{const r=await fetch('/raw/camera');if(!r.ok)throw new Error('raw HTTP '+r.status);const bytes=new Uint8Array(await r.arrayBuffer());return {bytes:bytes.length,hash:[...new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))],units:r.headers.get('X-Chiton-Units'),layout:r.headers.get('X-Chiton-Layout')};})()`),rawBefore=await raw();
        await b.evaluate(`${a}.view.cloud.nav.orbit(.5,.2);${a}.view.cloud.nav.pan(20,10);${a}.view.cloud.setPointStyle({shape:'square',diameterCssPx:10,sizing:'perspective'});${a}.view.cloud.dirty()`);
        assert.deepEqual(await raw(),rawBefore);assert.equal(rawBefore.bytes,640*480*2);assert.equal(rawBefore.layout,'processed_uint16_row_major');
        assert.deepEqual(await downloaded('obj'),obj);assert.equal(mock.counts['/pointcloud_obj/camera'],beforeOBJ+2);assert.equal(mock.source.mutations.length,0);
        for(const fault of ['409','503','json']){
            mock.control.obj=fault;const downloads=b.downloads.length,requests=mock.counts['/pointcloud_obj/camera'];await click('obj');
            await b.wait(`!${a}.state.objPending && document.getElementById('settings-result').textContent.includes(${JSON.stringify(fault==='json'?'metadata/type mismatch':'HTTP '+fault)})`);
            await b.sleep(100);assert.equal(b.downloads.length,downloads);assert.equal(mock.counts['/pointcloud_obj/camera'],requests+1);assert.equal(await b.evaluate('document.getElementById("obj").disabled'),false);
        }
        mock.control.obj='ok';
        const uploads=await b.evaluate(`${a}.view.cloud.uploads`);await b.sleep(1700);assert.equal(await b.evaluate(`${a}.view.cloud.uploads`),uploads);await b.wait('document.getElementById("image-state").textContent.includes("stale")');
        // Draw the same selected packed record into an actual offscreen framebuffer, not a CPU RGB array.
        const color=()=>b.evaluate(`(()=>{const v=${a}.view.cloud,T=THREE,index=${320*640+460},p=v.geometry.attributes.position,target=new T.WebGLRenderTarget(64,64),oldTarget=v.renderer.getRenderTarget(),oldRange={...v.geometry.drawRange},axes=v.axes.visible;const camera=new T.OrthographicCamera(-100,100,100,-100,.1,20000);camera.position.set(p.getX(index),p.getY(index),p.getZ(index)+10000);camera.lookAt(p.getX(index),p.getY(index),p.getZ(index));camera.updateMatrixWorld();try{v.axes.visible=false;v.geometry.setDrawRange(index,1);v.renderer.setRenderTarget(target);v.renderer.render(v.scene,camera);const pixel=new Uint8Array(4);v.renderer.readRenderTargetPixels(target,32,32,1,1,pixel);const actual=[...pixel.slice(0,3)].map(x=>x/255),expected=ChitonConsole.cloudColor(v.geometry.attributes.depth.getX(index),v.range);if(!actual.every((x,k)=>Math.abs(x-expected[k])<=2/255))throw new Error('Actual selected-point palette mismatch '+JSON.stringify({actual,expected}));return actual;}finally{v.geometry.setDrawRange(oldRange.start,oldRange.count);v.axes.visible=axes;v.renderer.setRenderTarget(oldTarget);target.dispose();v.dirty();}})()`),oldColor=await color(),rangePose=await pose(),distinct=await b.evaluate(`${a}.state.cloud.times.length`);
        mock.source.range={min:500,max:1500};await b.wait(`${a}.view.cloud.range?.min===500`);assert.notDeepEqual(await color(),oldColor);assert.deepEqual(await pose(),rangePose);assert.equal(await b.evaluate(`${a}.state.cloud.times.length`),distinct);
        mock.control.range='unavailable';await b.wait(`${a}.view.cloud.range===null && document.getElementById('cloud-legend').textContent.includes('unavailable')`);const neutral=await color();neutral.forEach((v,i)=>assert(Math.abs(v-[.6,.67,.73][i])<=2/255));assert.match(await b.evaluate('document.getElementById("cloud-legend").textContent'),/unavailable/);
        // PNG obtains its legend from the same rendered range; it cannot use a new status range over old colors.
        const neutralPNG=await downloaded();assert.equal(neutralPNG.subarray(0,8).toString('hex'),'89504e470d0a1a0a');assert.deepEqual(await pose(),rangePose);
        mock.control.range='ok';mock.source.range={min:0,max:3000};await b.wait(`${a}.view.cloud.range?.min===0`);
        mock.control.cloud='503';await b.wait(`${a}.state.cloud.error!=null`);assert.equal(await b.evaluate(`${a}.view.cloud.count`),307200);assert.equal(await b.evaluate(`${a}.state.phase`),'live');
        mock.control.cloud='empty';mock.source.seek(2000);await b.wait(`${a}.view.cloud?.count===0`);assert.equal(await b.evaluate(`${a}.view.cloud.geometry.drawRange.count`),0);
        const downloads=b.downloads.length;await click('png');await b.sleep(100);assert.equal(b.downloads.length,downloads);
        mock.control.cloud='single';mock.source.seek(2500);await b.wait(`${a}.view.cloud?.count===1`);assert((await b.evaluate(`${a}.view.cloud.nav.position()`)).every(Number.isFinite));
        mock.control.cloud='ok';mock.source.frameType='ir';mock.source.revision++;await b.wait(`${a}.state.device.frameType==='ir' && ${a}.view.cloud===null`);assert.equal(await b.evaluate(`${a}.cloud.running`),false);assert.match(await b.evaluate('document.getElementById("image-state").textContent'),/unavailable for IR/);
        assert.equal(await b.evaluate('document.getElementById("obj").disabled'),true);assert.equal(await b.evaluate(`${a}.state.cloud.latest`),null);assert.equal(await b.evaluate(`document.getElementById('view-mode').options[5].disabled`),true);
        mock.source.frameType='depth';mock.source.revision++;mock.source.seek(3000);await b.wait(`${a}.view.cloud?.count===307200`);
        mock.control.capture=false;await b.wait(`${a}.state.device.capture===false && document.getElementById('obj').disabled`);
        const disabledOBJ=mock.counts['/pointcloud_obj/camera'];await click('obj');assert.equal(mock.counts['/pointcloud_obj/camera'],disabledOBJ);
        mock.control.capture=true;await b.wait(`${a}.view.cloud?.count===307200 && !document.getElementById('obj').disabled`);
        mock.control.cloud='hanging-body';await b.sleep(550);assert(mock.active.cloud>=1);await b.evaluate(`Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'))`);await b.sleep(100);
        const reads=mock.requests.length;await b.sleep(250);assert.equal(mock.requests.length,reads);assert.equal(await b.evaluate(`${a}.view.cloud.raf`),null);assert.equal(mock.active.cloud,0);
        mock.control.cloud='ok';await b.evaluate(`Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'))`);await b.wait(`${a}.cloud.running`);
        await click('pause-viewer');assert.equal(await b.evaluate(`${a}.cloud.running`),false);await click('pause-viewer');await b.wait(`${a}.cloud.running`);
        mock.source.mode=1;await b.wait(`${a}.state.device.mode===1 && ${a}.view.lastImage!=null`);assert.equal(await b.evaluate(`${a}.view.cloud`),null);assert.equal(await b.evaluate('document.getElementById("obj").disabled'),true);const clouds=mock.counts['/pointcloud/camera'];await b.sleep(600);assert.equal(mock.counts['/pointcloud/camera'],clouds);
        assert.deepEqual(b.errors,[]);assert.equal(b.requests.some(u=>!u.startsWith(mock.origin+'/')&&!u.startsWith('data:')),false);
        fs.writeFileSync(path.join(out,'report.json'),JSON.stringify({passed:true,pixels,maximum:mock.maximum,errors:b.errors,counts:mock.counts},null,2));
        console.log('console_cloud_browser: real WebGL points, native PNG/OBJ downloads, OBJ bytes/scale/pose independence/errors/single-flight, mouse/touch/keyboard, fit/resize, 50 transitions, context loss/retry, live binary-only lifecycle, stale/empty/IR, hidden/pause cancellation passed');
    }finally{await b.close();await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
