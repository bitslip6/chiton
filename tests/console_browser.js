/* Actual-page tests; disposable Chromium and loopback fixture only. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture');const {browserFor}=require('../tools/console-browser-driver');
(async()=>{
    const mock=await fixture(),b=await browserFor(mock.origin),out=path.resolve('_build/console-review/browser');
    fs.mkdirSync(out,{recursive:true});
    const click=id=>b.evaluate(`document.getElementById(${JSON.stringify(id)}).click()`,true);
    const select=(id,value)=>b.evaluate(`document.getElementById(${JSON.stringify(id)}).value=${JSON.stringify(String(value))};document.getElementById(${JSON.stringify(id)}).dispatchEvent(new Event('change'));`);
    const app='ChitonConsoleApp';
    async function downloaded(fn){
        const before=b.downloads.filter(v=>v.method==='Browser.downloadWillBegin').length;await fn();
        for(let i=0;i<100;i++){const starts=b.downloads.filter(v=>v.method==='Browser.downloadWillBegin');const event=starts[before];if(event&&b.downloads.some(v=>v.method==='Browser.downloadProgress'&&v.params.guid===event.params.guid&&v.params.state==='completed')){assert.equal(starts.length,before+1);return {filename:event.params.suggestedFilename,bytes:fs.readFileSync(path.join(out,event.params.guid))};}await b.sleep(30);}throw new Error('Native download timed out');
    }
    try{
        await b.command('Browser.setDownloadBehavior',{behavior:'allowAndName',downloadPath:out,eventsEnabled:true},null);
        await b.viewport(1280,720);await b.navigate(mock.origin+'/console.html?source=demo');
        await b.wait(`${app}.state.latest?.count>=10`);
        assert.equal(mock.requests.filter(v=>!/^\/[\w.-]+\.(js|css|html|png)$/.test(v)).length,0);assert.equal(mock.clients.size,0);
        assert.match(await b.evaluate('document.getElementById("source-badge").textContent'),/DEMO/);
        await b.evaluate(`${app}.prepareCapture({filter:true})`);
        assert(await b.evaluate(`${app}.state.estimate!=null`));
        assert.equal(await b.evaluate('document.getElementById("quality-settings").querySelector("button").disabled'),true);
        const colorChanged=await b.evaluate(`(async()=>{const a=${app},draw=()=>{const m=a.source.metadata();a.view.drawFrame({synthetic:true,source:a.source,metadata:m},{id:m.frame_id,mode:m.color_mode,age:m.frame_age_ms},a.state);return [...a.view.sensor.getContext('2d').getImageData(460,320,1,1).data];};await a.source.applySetting({name:'mode',value:3});const before=draw();await a.source.applySetting({name:'range',value:{min:500,max:1500}});const after=draw();await a.source.applySetting({name:'range',value:{min:0,max:3000}});await a.source.applySetting({name:'mode',value:1});return JSON.stringify(before)!==JSON.stringify(after);})()`);
        assert.equal(colorChanged,true);
        await b.evaluate(`${app}.prepareCapture({filter:true})`);
        const stats=await b.evaluate(`({api:${app}.state.latest.stddev,local:${app}.state.history.items.length})`);assert(stats.api>0);assert(stats.local<=300);
        await click('reference');await b.wait('document.getElementById("delta").textContent==="0"');
        const selection=await b.evaluate(`(()=>{const r=document.getElementById('sensor').getBoundingClientRect();document.getElementById('sensor').dispatchEvent(new PointerEvent('pointerdown',{clientX:r.x+r.width/2,clientY:r.y+r.height/2,button:0,bubbles:true}));return ${app}.state.selection;})()`);
        assert.deepEqual(selection,{kind:'point',x:320,y:240});assert.equal(await b.evaluate(`${app}.state.reference`),null);
        await b.evaluate(`document.getElementById('sensor').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true}));`);assert.equal(await b.evaluate(`${app}.state.selection.x`),321);
        await b.viewport(390,844,2);assert.equal(await b.evaluate('document.documentElement.scrollWidth<=innerWidth'),true);
        assert.equal(await b.evaluate(`${app}.state.selection.x`),321);
        await click('roi-mode');await b.evaluate(`${app}.prepareCapture({timeMs:3000})`);assert.equal(await b.evaluate(`${app}.state.latest.statisticsKind`),'spatial');assert.equal(await b.evaluate('document.getElementById("filter").disabled'),true);
        // Cancellation must resume collection, even without a valid drag endpoint.
        await b.evaluate(`${app}.source.resume();${app}.start();`);
        const dragPosition=await b.evaluate(`(()=>{const r=document.getElementById('sensor').getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
        await b.command('Input.dispatchMouseEvent',{type:'mousePressed',...dragPosition,button:'left',clickCount:1});
        await b.evaluate(`document.getElementById('sensor').dispatchEvent(new PointerEvent('pointercancel',{pointerId:1}));`);
        await b.command('Input.dispatchMouseEvent',{type:'mouseReleased',...dragPosition,button:'left',clickCount:1});
        assert.equal(await b.evaluate(`${app}.measurement.running`),true);
        await b.evaluate(`${app}.prepareCapture({timeMs:4000})`);
        await click('open-settings');assert.equal(await b.evaluate('document.getElementById("settings").open'),true);
        // Native modal traps Tab, closes on Escape and restores trigger focus.
        await b.command('Input.dispatchKeyEvent',{type:'keyDown',key:'Tab',code:'Tab',windowsVirtualKeyCode:9});
        assert.equal(await b.evaluate('document.getElementById("settings").contains(document.activeElement)'),true);
        await b.command('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await b.command('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
        await b.wait('!document.getElementById("settings").open && document.activeElement.id==="open-settings"');
        const csv=await downloaded(()=>click('csv'));assert.match(csv.filename,/demo.*csv/);assert.match(csv.bytes.toString(),/capturedAtMs/);assert.match(csv.bytes.toString(),/"demo"/);
        const png=await downloaded(()=>click('png'));assert.equal(png.bytes.subarray(0,8).toString('hex'),'89504e470d0a1a0a');assert.equal(png.bytes.readUInt32BE(16),640);
        await b.screenshot(path.join(out,'mobile-roi.png'));
        // Long-running accelerated collection remains bounded; no request-count IDs.
        await b.evaluate(`(async()=>{const a=${app};a.stop();a.source.seek(0);a.setSelection({kind:'point',x:460,y:320});const start=performance.now();for(let i=0;i<5000;i++){a.source.seek(i*50);a.accept(await a.source.readMeasurement({clientId:'long_run',selection:a.state.selection}),{latency:0},start+i*50);}return a.state.history.items.length;})()`);
        assert.equal(await b.evaluate(`${app}.state.history.items.length`),300);assert.equal(await b.evaluate(`${app}.source.maximum`),1);
        // Live mode: real wire JPEG protocol, never demo values or startup writes.
        await b.viewport(1280,720);mock.requests.length=0;await b.navigate(mock.origin+'/console.html?source=live');
        await b.wait(`${app}.state.latest?.count>=10 && ${app}.view.lastImage!=null`);
        assert.equal(await b.evaluate(`${app}.state.latest.source`),'live');assert.equal(mock.source.mutations.length,0);assert.equal(mock.clients.size,1);
        assert.equal(mock.maximum.measurement,1);assert(mock.counts['/image/camera']===undefined); // WS, not a duplicate HTTP loop
        mock.control.measurement='503';await b.wait(`${app}.state.phase==='error'`);
        const first=await b.evaluate(`${app}.state.latest.frameId`);await b.sleep(200);assert.equal(await b.evaluate(`${app}.state.latest.frameId`),first);
        const imageBefore=await b.evaluate(`${app}.view.lastImage.id`);await b.sleep(200);assert((await b.evaluate(`${app}.view.lastImage.id`))>imageBefore);
        mock.control.measurement='ok';await b.wait(`${app}.state.phase==='live'`);
        mock.control.image='503';await b.sleep(300);assert.equal(await b.evaluate(`${app}.state.phase`),'live');mock.control.image='ok';
        await b.wait(`!!${app}.view.lastImage`); // HTTP fallback while stream cooldown is active
        await b.wait(`document.getElementById('image-state').textContent==='Image live'`);
        assert(mock.counts['/image/camera']>0);
        // External global-mode following; no browser mutation.
        await mock.source.applySetting({name:'mode',value:5});await b.wait(`${app}.state.device.mode===5 && ${app}.view.sensor.width===1500`);
        assert.equal(await b.evaluate(`${app}.state.selection.y`),82);assert.equal(await b.evaluate('document.getElementById("roi-mode").disabled'),true);
        await mock.source.applySetting({name:'mode',value:4});await b.wait(`${app}.state.device.mode===4`);const images=mock.counts['/image/camera'];await b.sleep(200);assert.equal(mock.counts['/image/camera'],images);assert.equal(mock.clients.size,0);
        await mock.source.applySetting({name:'mode',value:1});await b.wait(`${app}.state.device.mode===1 && ${app}.state.latest!=null`);
        // Lost mutation ack: apply once, outcome unknown, observational read follows.
        mock.control.mutationAck='fail-after-apply';await b.command('Page.handleJavaScriptDialog',{accept:true}).catch(()=>{}); // dialog is not open yet
        await b.evaluate('window.confirm=()=>true');await select('view-mode',3);
        await b.wait(`${app}.state.device.mode===3`);assert.equal(mock.counts['/colormode/setting/3'],1);mock.control.mutationAck='ok';
        // Frozen successful reads are stale and contribute no graph/filter records.
        mock.source.seek(20000);await b.wait(`${app}.state.latest?.frameId===601`);await b.sleep(250);
        const size=await b.evaluate(`${app}.state.history.items.length`);await b.sleep(600);assert.equal(await b.evaluate(`${app}.state.history.items.length`),size);
        // A frozen fixture's age still increases independently of repeated reads.
        mock.source.metadata=((original)=>function(){const m=original.call(this);return {...m,frame_age_ms:2000};})(mock.source.metadata);
        await b.wait('document.getElementById("measurement-state").textContent.includes("stale")');
        await b.wait('document.getElementById("image-state").textContent.includes("stale")').catch(async error=>{console.error(await b.evaluate(`({last:ChitonConsoleApp.view.lastImage,now:performance.now(),frozen:ChitonConsoleApp.state.frozenNow,text:document.getElementById('image-state').textContent})`));throw error;});
        // Hidden page and viewer pause close sockets and abort all scheduled reads.
        await b.evaluate(`Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'));`);await b.sleep(100);
        assert.equal(mock.clients.size,0);assert.equal(await b.evaluate(`${app}.measurement.running`),false);const reads=Object.values(mock.counts).reduce((a,v)=>a+v,0);await b.sleep(200);assert.equal(Object.values(mock.counts).reduce((a,v)=>a+v,0),reads);
        await b.evaluate(`Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'));`);await b.wait(`${app}.measurement.running`);
        await click('pause-viewer');assert.equal(await b.evaluate(`${app}.state.phase`),'paused');await click('pause-viewer');await b.wait(`${app}.measurement.running`);
        const raw=await downloaded(()=>click('raw'));assert.equal(raw.filename,'chiton-raw.bin');assert.equal(raw.bytes.length,614400);
        mock.control.capture=false;const downloads=b.downloads.length;await click('raw');await b.sleep(250);assert.equal(b.downloads.length,downloads);
        await b.screenshot(path.join(out,'live-stale.png'));
        // Bad sources fail closed before starting any adapter.
        const mutations=mock.source.mutations.length;await b.command('Page.navigate',{url:mock.origin+'/console.html?source=invalid'});await b.wait('document.getElementById("notice").textContent.includes("Unknown source")');assert.equal(mock.source.mutations.length,mutations);
        assert.deepEqual(b.errors,[]);assert.equal(b.requests.some(u=>!u.startsWith(mock.origin+'/')&&!u.startsWith('data:')),false);
        console.log('console_browser: native demo/live, WS/HTTP fallback, mapping/ROI, focus, CSV/PNG/raw downloads, bounded 5000-frame run, faults, global modes, stale/frozen, hidden/resume, lost ack passed');
        fs.writeFileSync(path.join(out,'report.json'),JSON.stringify({passed:true,errors:b.errors,maximum:mock.maximum,requestCounts:mock.counts,downloads:b.downloads.filter(v=>v.method==='Browser.downloadWillBegin').map(v=>v.params.suggestedFilename)},null,2));
    }finally{await b.close();await mock.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
