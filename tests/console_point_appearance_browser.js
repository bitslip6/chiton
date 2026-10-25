/* Point appearance controller integration: real loopback Chromium, never a camera. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
(async()=>{
    const mock=await fixture(),b=await browserFor(mock.origin),out=path.resolve('_build/pointcloud-renderer-style/validation/ui');fs.mkdirSync(out,{recursive:true});
    const app='ChitonConsoleApp',defaults={shape:'circle',diameterCssPx:2.5,sizing:'screen'},selected={shape:'square',diameterCssPx:7.5,sizing:'perspective'},captures=[],layouts=[];
    const click=id=>b.evaluate(`document.getElementById(${JSON.stringify(id)}).click()`,true);
    const paint=()=>b.evaluate('new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(()=>setTimeout(r,80))))');
    const style=()=>b.evaluate(`${app}.view.cloud.pointStyle`),pose=()=>b.evaluate(`${app}.view.cloud.nav.pose()`);
    async function choose(value){await b.evaluate(`(()=>{const s=${JSON.stringify(value)};for(const [id,v,event] of [['point-shape',s.shape,'change'],['point-diameter',s.diameterCssPx,'input'],['point-sizing',s.sizing,'change']]){const e=document.getElementById(id);e.value=v;e.dispatchEvent(new Event(event,{bubbles:true}));}})()`);await paint();}
    async function instrument(){await b.evaluate(`(()=>{const v=${app}.view.cloud,gl=v.renderer.getContext();window.pointProbe={uploads:0,paints:0};for(const key of ['bufferData','bufferSubData']){const old=gl[key];gl[key]=function(...args){pointProbe.uploads++;return old.apply(this,args);};}const render=v.renderer.render;v.renderer.render=function(...args){pointProbe.paints++;return render.apply(this,args);};})()`);}
    async function downloadPNG(shape){
        const before=b.downloads.filter(e=>e.method==='Browser.downloadWillBegin').length;
        await b.evaluate(`(()=>{window.pngFooter=[];const original=CanvasRenderingContext2D.prototype.fillText;CanvasRenderingContext2D.prototype.fillText=function(...args){pngFooter.push(String(args[0]));return original.apply(this,args);};window.restorePNGText=()=>{CanvasRenderingContext2D.prototype.fillText=original;};document.getElementById('png-overlays').checked=false;})()`);
        try{
            await click('png');let event;
            for(let i=0;i<200;i++){
                event=b.downloads.filter(e=>e.method==='Browser.downloadWillBegin')[before];
                if(event&&b.downloads.some(e=>e.method==='Browser.downloadProgress'&&e.params.guid===event.params.guid&&e.params.state==='completed'))break;
                await b.sleep(30);
            }
            assert(event,'PNG download missing');assert(b.downloads.some(e=>e.method==='Browser.downloadProgress'&&e.params.guid===event.params.guid&&e.params.state==='completed'));
            assert.equal(b.downloads.filter(e=>e.method==='Browser.downloadWillBegin').length,before+1);
            const bytes=fs.readFileSync(path.join(out,event.params.guid));assert.equal(bytes.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
            const footer=await b.evaluate(`pngFooter.filter(t=>t.startsWith('Style:'))`);assert.equal(footer.length,1);assert.match(footer[0],new RegExp(`Style: ${shape} 2.5 CSS px.*screen.*DPR.*HW`));
            fs.writeFileSync(path.join(out,shape+'-export.png'),bytes);return {footer:footer[0],bytes:bytes.length,width:bytes.readUInt32BE(16),height:bytes.readUInt32BE(20)};
        }finally{await b.evaluate('restorePNGText()');}
    }
    const pixels=()=>b.evaluate(`(()=>{const v=${app}.view.cloud;v.render();const gl=v.renderer.getContext(),p=new Uint8Array(v.canvas.width*v.canvas.height*4);gl.readPixels(0,0,v.canvas.width,v.canvas.height,gl.RGBA,gl.UNSIGNED_BYTE,p);let hash=2166136261;for(const x of p)hash=Math.imul(hash^x,16777619);return hash>>>0;})()`);
    try{
        await b.command('Browser.setDownloadBehavior',{behavior:'allowAndName',downloadPath:out,eventsEnabled:true},null);
        await b.viewport(1280,720);await b.navigate(mock.origin+'/console.html?source=demo&scenario=cloud');await b.wait(`${app}.view.cloud?.count===307200`);
        let evidence=await b.evaluate(`${app}.prepareCapture({cloudMode:true,filter:true})`);await paint();assert.deepEqual(await style(),defaults);assert.deepEqual(evidence.cloud.pointStyle,defaults);
        assert.equal(await b.evaluate(`document.querySelectorAll('#point-appearance').length===1 && !document.getElementById('point-appearance-controls').disabled`),true);
        await b.evaluate(`document.getElementById('toggle-display-controls').click();document.getElementById('point-appearance').open=true`);
        await b.evaluate(`${app}.view.cloud.nav.orbit(.3,.1);${app}.view.cloud.dirty()`);await paint();
        const storage=await b.evaluate('JSON.stringify(localStorage)'),initialPose=await pose();
        const measurement=()=>b.evaluate(`JSON.stringify({latest:${app}.state.latest,history:${app}.state.history.items,kalman:${app}.state.kalman,estimate:${app}.state.estimate,reference:${app}.state.reference,selection:${app}.state.selection})`),beforeMeasurement=await measurement();
        assert.equal(await b.evaluate(`${app}.state.filter`),'Balanced');assert((await b.evaluate(`${app}.state.history.items.length`))>0);
        await instrument();const baseline=await b.evaluate(`${app}.view.cloud.counters()`),beforePixels=await pixels();
        await choose(selected);assert.deepEqual(await style(),selected);assert.deepEqual(await pose(),initialPose);assert.notEqual(await pixels(),beforePixels);
        assert.equal(await b.evaluate('pointProbe.uploads'),0);assert.equal(await b.evaluate(`${app}.view.cloud.uploads`),baseline.geometryUploads);assert.equal(await measurement(),beforeMeasurement);
        assert.equal(await b.evaluate('JSON.stringify(localStorage)'),storage);assert.equal(mock.source.mutations.length,0);
        // Native slider arrows work, including when the same group lives inside #viewport.
        await b.evaluate(`document.getElementById('point-diameter').focus()`);
        await b.command('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});
        await b.command('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});
        assert.equal((await style()).diameterCssPx,8);assert.deepEqual(await pose(),initialPose);
        const readout=await b.evaluate(`document.getElementById('point-diameter-value').getBoundingClientRect().width`);
        await choose({...selected,diameterCssPx:10});assert.equal(await b.evaluate(`document.getElementById('point-diameter-value').getBoundingClientRect().width`),readout);
        await click('reset-appearance');assert.deepEqual(await style(),defaults);assert.deepEqual(await pose(),initialPose);
        await choose(selected);
        await click('fullscreen');await b.wait(`document.fullscreenElement?.id==='viewport' && document.getElementById('point-appearance').parentElement.id==='display-cloud-controls' && document.getElementById('cloud-navigation').parentElement.id==='display-navigation' && document.getElementById('display-controls').parentElement.id==='viewport'`);
        await b.evaluate(`document.getElementById('point-sizing').focus()`);
        await b.command('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowUp',code:'ArrowUp',windowsVirtualKeyCode:38});
        await b.command('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowUp',code:'ArrowUp',windowsVirtualKeyCode:38});
        assert.equal((await style()).sizing,'screen');assert.deepEqual(await pose(),initialPose);
        await b.evaluate(`document.getElementById('point-diameter').focus()`);
        await b.command('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowLeft',code:'ArrowLeft',windowsVirtualKeyCode:37});
        await b.command('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowLeft',code:'ArrowLeft',windowsVirtualKeyCode:37});
        assert.equal((await style()).diameterCssPx,7);assert.deepEqual(await pose(),initialPose);
        assert.equal(await b.evaluate(`document.querySelectorAll('#point-appearance').length`),1);
        for(const [width,height] of [[320,844],[640,360]]){
            await b.viewport(width,height,2);await paint();
            const fits=await b.evaluate(`(()=>{const g=document.getElementById('display-controls'),r=g.getBoundingClientRect();return r.left>=0 && r.right<=innerWidth && r.top>=0 && r.bottom<=innerHeight && g.scrollWidth<=g.clientWidth;})()`);
            assert.equal(fits,true,`Fullscreen appearance overflow at ${width}px`);
        }
        await b.viewport(1280,720);await paint();await b.screenshot(path.join(out,'fullscreen.png'));await b.evaluate('document.exitFullscreen()',true);
        await b.wait(`document.getElementById('point-appearance').parentElement.id==='display-cloud-controls'`);
        // Expanded controls at required widths / font modes, including 200%-equivalent reflow.
        for(const [width,height,dpr] of [[1920,1080,1],[1280,720,1],[390,844,2],[320,844,1],[640,360,2]]){
            await b.viewport(width,height,dpr);
            for(const size of ['normal','large','presentation']){
                await b.evaluate(`document.body.dataset.size=${JSON.stringify(size)}`);await paint();
                const layout=await b.evaluate(`(()=>{const g=document.getElementById('point-appearance'),r=g.getBoundingClientRect();return {overflow:document.documentElement.scrollWidth>innerWidth,clipped:[...g.querySelectorAll('select,input,output,button')].filter(e=>{const b=e.getBoundingClientRect();return b.left<r.left||b.right>r.right+1||b.top<r.top||b.bottom>r.bottom;}).map(e=>e.id)};})()`);
                assert.equal(layout.overflow,false,JSON.stringify({width,size,layout}));assert.deepEqual(layout.clipped,[]);layouts.push({width,height,dpr,size,...layout});
            }
        }
        await b.viewport(1280,720);await b.evaluate(`document.body.dataset.size='normal'`);await choose(selected);
        // Pausing keeps local settings editable without any actual WebGL paint or acquisition.
        await click('pause-viewer');await paint();const paints=await b.evaluate('pointProbe.paints'),requests=mock.requests.length;
        await choose({...defaults,diameterCssPx:4});await b.sleep(150);
        assert.equal(await b.evaluate('pointProbe.paints'),paints);assert.equal(await b.evaluate(`${app}.view.cloud.raf`),null);assert.equal(await b.evaluate(`${app}.cloud.running`),false);assert.equal(mock.requests.length,requests);
        await click('pause-viewer');await b.wait(`${app}.cloud.running`);assert.deepEqual(await style(),{...defaults,diameterCssPx:4});
        await choose(selected);
        for(let i=0;i<3;i++){
            await b.evaluate(`(async()=>{const a=${app};a.source.mode=1;a.applyStatus(await a.source.readStatus());a.source.mode=4;a.applyStatus(await a.source.readStatus());})()`);
            await b.wait(`${app}.view.cloud?.count===307200`);assert.deepEqual(await style(),selected);
        }
        await b.evaluate(`Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'))`);
        await choose(defaults);assert.equal(await b.evaluate(`${app}.view.cloud.raf`),null);assert.equal(await b.evaluate(`${app}.cloud.running`),false);
        await b.evaluate(`Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'))`);await b.wait(`${app}.cloud.running`);assert.deepEqual(await style(),defaults);
        await choose(selected);
        await b.evaluate(`(async()=>{const a=${app};a.source.revision++;a.applyStatus(await a.source.readStatus());})()`);await b.wait(`${app}.view.cloud?.count===307200`);assert.deepEqual(await style(),selected);
        await b.evaluate(`${app}.view.cloud.renderer.getContext().getExtension('WEBGL_lose_context').loseContext()`);await b.wait(`${app}.state.cloudUnavailable!=null`);
        assert.equal(await b.evaluate(`document.getElementById('point-appearance-controls').disabled`),true);
        await click('cloud-retry');await b.wait(`${app}.view.cloud?.count===307200`);assert.deepEqual(await style(),selected);
        // Capture option is validated, copied, demo-only, and applied even on fresh construction.
        assert.equal(await b.evaluate(`(async()=>{try{await ${app}.prepareCapture({pointStyle:{shape:'square'}});return false;}catch(e){return true;}})()`),true);
        await b.evaluate(`(async()=>{const a=${app};a.source.mode=1;a.applyStatus(await a.source.readStatus());})()`);
        evidence=await b.evaluate(`${app}.prepareCapture({cloudMode:true,pointStyle:${JSON.stringify(selected)}})`);
        assert.deepEqual(evidence.cloud.pointStyle,selected);assert.deepEqual(evidence.cloud.renderer.pointStyle,selected);assert.deepEqual(await style(),selected);captures.push(evidence);
        for(const shape of ['square','circle']){
            evidence=await b.evaluate(`${app}.prepareCapture({cloudMode:true,pointStyle:{shape:'${shape}',diameterCssPx:2.5,sizing:'screen'}})`);await paint();
            assert.equal(evidence.cloud.renderer.pointStyle.shape,shape);evidence.png=await downloadPNG(shape);captures.push(evidence);await b.screenshot(path.join(out,shape+'.png'));
        }
        assert.equal(mock.source.mutations.length,0);
        // Frozen real live IDs: direct range setter/legend and style updates never upload attributes.
        mock.source.mode=4;mock.source.seek(1000);await b.navigate(mock.origin+'/console.html?source=live');await b.wait(`${app}.view.cloud?.count===307200 && ${app}.state.cloud.latest?.frameId===31`);await paint();
        assert.deepEqual(await style(),defaults);await instrument();const frozen=await b.evaluate(`({counters:${app}.view.cloud.counters(),times:${app}.state.cloud.times.length,records:[...${app}.state.cloud.latest.records.slice(0,40)]})`),frozenPose=await pose(),oldPixels=await pixels();
        mock.source.range={min:500,max:1500};await b.wait(`${app}.view.cloud.range?.min===500 && document.getElementById('cloud-legend').textContent.includes('500…1500')`);await paint();assert.notEqual(await pixels(),oldPixels);
        await choose(selected);mock.control.range='unavailable';await b.wait(`${app}.view.cloud.range===null && document.getElementById('cloud-legend').textContent.includes('unavailable')`);await paint();
        assert.equal(await b.evaluate('pointProbe.uploads'),0);assert.equal(await b.evaluate(`${app}.view.cloud.uploads`),frozen.counters.geometryUploads);assert.deepEqual(await pose(),frozenPose);
        assert.equal(await b.evaluate(`${app}.state.cloud.times.length`),frozen.times);assert.deepEqual(await b.evaluate(`[...${app}.state.cloud.latest.records.slice(0,40)]`),frozen.records);
        assert.match(await b.evaluate(`document.getElementById('cloud-resources').textContent`),/geometry upload requests.*requested attribute B.*changed uniform sets.*effective DPR.*hardware point diameter/);
        assert.equal(await b.evaluate(`(async()=>{try{await ${app}.prepareCapture({pointStyle:${JSON.stringify(selected)}});return false;}catch(e){return e.message.includes('demo');}})()`),true);
        // Live paused changes produce no network, GL upload or paint, and persist on resume.
        await click('pause-viewer');await paint();const liveReads=mock.requests.length,livePaints=await b.evaluate('pointProbe.paints');await choose(defaults);await b.sleep(300);
        assert.equal(mock.requests.length,liveReads);assert.equal(await b.evaluate('pointProbe.paints'),livePaints);assert.equal(await b.evaluate('pointProbe.uploads'),0);
        await click('pause-viewer');await b.wait(`${app}.cloud.running`);assert.deepEqual(await style(),defaults);
        mock.source.frameType='ir';mock.source.revision++;await b.wait(`${app}.view.cloud===null && ${app}.state.device.frameType==='ir'`);
        assert.equal(await b.evaluate(`document.getElementById('point-appearance-controls').disabled && document.getElementById('point-appearance-note').textContent.includes('unavailable')`),true);
        assert.equal(mock.source.mutations.length,0);assert.deepEqual(b.errors,[]);assert.equal(b.requests.some(u=>!u.startsWith(mock.origin+'/')&&!u.startsWith('data:')),false);
        fs.writeFileSync(path.join(out,'report.json'),JSON.stringify({passed:true,captures,layouts,frozen,maximum:mock.maximum,errors:b.errors,mutations:mock.source.mutations},null,2));
        console.log('console_point_appearance_browser: controls/reset/keyboard/fullscreen, 15 expanded reflow variants, mode/context/hide/pause/retry persistence, capture styles, actual frozen GL uploads/pixels and no device writes passed');
    }finally{await b.close();await mock.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
