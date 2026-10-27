/* Invoked by console_native.py only after identifying the isolated SDK-free stub. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const {browserFor}=require('../tools/console-browser-driver');
(async()=>{
    const origin=process.argv[2];if(!origin)throw new Error('Use python3 tests/console_native.py /path/to/stub');
    const browser=await browserFor(origin);
    try{
        await browser.viewport(1280,720);await browser.navigate(origin+'/console.html');
        await browser.wait('ChitonConsoleApp.state.latest?.value===800 && ChitonConsoleApp.view.lastImage!=null',15000);
        assert.equal(await browser.evaluate('ChitonConsoleApp.state.latest.units'),'depth_sensor_units');
        await browser.evaluate('ChitonConsoleApp.setSelection({kind:"roi",x:450,y:300,x2:470,y2:330})');
        await browser.wait('ChitonConsoleApp.state.latest?.statisticsKind==="spatial"');
        assert.equal(await browser.evaluate('ChitonConsoleApp.state.latest.count'),600);
        const out=path.resolve(process.env.CHITON_CONSOLE_NATIVE_OUT||'_build/console-review/native');fs.mkdirSync(out,{recursive:true});await browser.screenshot(path.join(out,'stub-live.png'));
        await browser.evaluate(`ChitonConsoleApp.source.applySetting({name:'mode',value:4})`);
        await browser.wait('ChitonConsoleApp.state.device.mode===4 && ChitonConsoleApp.view.cloud?.count>0',15000);
        assert.equal(await browser.evaluate('ChitonConsoleApp.state.selection.kind'),'point');
        const cloud=await browser.evaluate(`(async()=>{const a=ChitonConsoleApp,m=await a.source.readCloud();return {frame:m.frameId,count:m.recordCount,bytes:m.bytes,units:m.units,scope:m.unitsScope,first:[...m.records.slice(0,4)],rendered:a.view.cloud.count,packed:[...a.view.cloud.records.slice(0,4)],position:[a.view.cloud.geometry.attributes.position.getX(0),a.view.cloud.geometry.attributes.position.getY(0),a.view.cloud.geometry.attributes.position.getZ(0)],depth:a.view.cloud.geometry.attributes.depth.getX(0),signed:a.view.cloud.records instanceof Int16Array,normalized:a.view.cloud.geometry.attributes.position.normalized,resources:a.view.cloud.counters()};})()`);
        assert(cloud.count>0&&cloud.count<=640*480);assert.equal(cloud.bytes,128+cloud.count*9);assert.equal(cloud.rendered,cloud.count);assert.deepEqual(cloud.packed,cloud.first);assert.deepEqual(cloud.position,cloud.first.slice(0,3));assert.equal(cloud.depth,cloud.first[3]);assert.equal(cloud.signed,true);assert.equal(cloud.normalized,false);assert.equal(cloud.resources.bufferBytes,640*480*8);assert.equal(cloud.units,'depth_sensor_units');
        await browser.screenshot(path.join(out,'stub-cloud.png'));
        await browser.command('Browser.setDownloadBehavior',{behavior:'allowAndName',downloadPath:out,eventsEnabled:true},null);
        await browser.evaluate(`document.getElementById('toggle-display-controls').click();document.getElementById('display-export').open=true`);
        assert.equal(await browser.evaluate('!document.getElementById("obj").disabled && document.getElementById("obj").getBoundingClientRect().width>0'),true);
        await browser.evaluate(`window.nativeRateStart={time:performance.now(),measurement:ChitonConsoleApp.measurement.requests,status:ChitonConsoleApp.status.requests,cloud:ChitonConsoleApp.cloud.requests};document.getElementById('cloud-rate').value='30';document.getElementById('cloud-rate').dispatchEvent(new Event('change'))`);
        await browser.sleep(4000);
        const highRate=await browser.evaluate(`({elapsed:performance.now()-nativeRateStart.time,measurement:ChitonConsoleApp.measurement.requests-nativeRateStart.measurement,status:ChitonConsoleApp.status.requests-nativeRateStart.status,cloud:ChitonConsoleApp.cloud.requests-nativeRateStart.cloud,distinct:ChitonConsoleApp.state.cloud.hz(performance.now()),render:ChitonConsoleApp.view.cloud.counters().renderFPS,phase:ChitonConsoleApp.state.phase,error:ChitonConsoleApp.state.cloud.error})`);
        assert(highRate.measurement>=20&&highRate.status>=2,'Native measurement/status starvation: '+JSON.stringify(highRate));assert.equal(highRate.phase,'live');assert.equal(highRate.error,null);
        await browser.evaluate(`document.getElementById('cloud-rate').value='2';document.getElementById('cloud-rate').dispatchEvent(new Event('change'))`);
        const exportHash=(route='/raw/camera')=>browser.evaluate(`(async()=>{const r=await fetch(${JSON.stringify(route)});if(!r.ok)throw new Error('raw HTTP '+r.status);const b=await r.arrayBuffer();return {bytes:b.byteLength,hash:[...new Uint8Array(await crypto.subtle.digest('SHA-256',b))],units:r.headers.get('X-Chiton-Units'),layout:r.headers.get('X-Chiton-Layout')};})()`),beforeRaw=await exportHash(),beforeOBJ=await exportHash('/pointcloud_obj/camera'),beforeBinary=await exportHash('/pointcloud/camera');
        // Actual console -> allocation-free C++ configuration API. Sensor-unit
        // frames must stay unfiltered even when the operator requests Hide.
        await browser.evaluate(`(()=>{document.getElementById('cleanup-enabled').checked=true;document.getElementById('cleanup-algorithm').value='3';document.getElementById('cleanup-boundary_gap').value='300';document.getElementById('cleanup-form').requestSubmit();document.getElementById('cleanup-display').value='hide';document.getElementById('cleanup-display').dispatchEvent(new Event('change'));})()`);
        await browser.wait('ChitonConsoleApp.state.cleanup.ack?.generation===2 && ChitonConsoleApp.view.cloud?.cleanupSnapshot?.generation===2',15000);
        assert.equal(await browser.evaluate('ChitonConsoleApp.view.cloud.cleanupSnapshot.status'),3);
        assert.equal(await browser.evaluate('ChitonConsoleApp.view.cloud.material.uniforms.cleanupMode.value'),0);
        assert.equal(await browser.evaluate('ChitonConsoleApp.state.cleanup.ack.config.boundary_gap'),300);
        assert.equal(await browser.evaluate('ChitonConsoleApp.state.cleanup.ack.config.algorithm'),3);
        await browser.evaluate(`(()=>{document.getElementById('cleanup-enabled').checked=false;document.getElementById('cleanup-form').requestSubmit();document.getElementById('cleanup-display').value='off';document.getElementById('cleanup-display').dispatchEvent(new Event('change'));})()`);
        await browser.wait('ChitonConsoleApp.state.cleanup.ack?.generation===3 && ChitonConsoleApp.view.cloud?.cleanupSnapshot?.generation===3',15000);
        assert.deepEqual(await exportHash('/pointcloud/camera'),beforeBinary);assert.deepEqual(await exportHash(),beforeRaw);
        const clipping=await browser.evaluate(`(()=>{const a=ChitonConsoleApp,C=ChitonConsole,m=a.state.cloud.latest,s=C.cloudClipBounds(m.records);s.depth.enabled=true;s.box.enabled=true;s.box.max[0]=Math.floor((s.box.min[0]+s.box.max[0])/2);document.getElementById('clip-details').open=true;document.getElementById('clip-depth-enabled').checked=true;document.getElementById('clip-box-enabled').checked=true;for(const end of ['min','max']){document.getElementById('clip-d-'+end).value=s.depth[end];for(let k=0;k<3;k++)document.getElementById('clip-'+'xyz'[k]+'-'+end).value=s.box[end][k];}document.getElementById('clip-form').requestSubmit();return {settings:s,expected:C.summarizeCloudClipping(m.records,s),counters:a.view.cloud.counters()};})()`);
        assert(clipping.expected.matchingCount>0&&clipping.expected.matchingCount<cloud.count);assert.equal(clipping.counters.matchingCount,clipping.expected.matchingCount);assert.equal(clipping.counters.submittedCount,cloud.count);assert.deepEqual(clipping.counters.clipSettings,clipping.settings);
        await browser.evaluate(`ChitonConsoleApp.view.cloud.nav.orbit(.5,.2);ChitonConsoleApp.view.cloud.setPointStyle({shape:'square',diameterCssPx:7.5,sizing:'perspective'});document.getElementById('obj').click()`,true);
        await browser.evaluate(`document.getElementById('cloud-fit-main').click();document.getElementById('cloud-projection').value='orthographic';document.getElementById('cloud-projection').dispatchEvent(new Event('change'));document.getElementById('cloud-edl').click()`,true);
        await browser.wait('ChitonConsoleApp.view.cloud?.eyeDomeLighting && ChitonConsoleApp.view.cloud.edl!==null');
        assert.equal(await browser.evaluate('ChitonConsoleApp.view.cloud.projection'),'orthographic');
        assert.deepEqual(await exportHash('/pointcloud_obj/camera'),beforeOBJ);assert.deepEqual(await exportHash('/pointcloud/camera'),beforeBinary);
        assert.deepEqual(await exportHash(),beforeRaw);assert.equal(beforeRaw.bytes,640*480*2);assert.equal(beforeRaw.layout,'processed_uint16_row_major');
        let downloaded=null;
        for(let i=0;i<200;i++){
            const starts=browser.downloads.filter(e=>e.method==='Browser.downloadWillBegin'),event=starts[0];
            if(event&&browser.downloads.some(e=>e.method==='Browser.downloadProgress'&&e.params.guid===event.params.guid&&e.params.state==='completed')){assert.equal(starts.length,1);downloaded=event.params;break;}
            await browser.sleep(30);
        }
        assert(downloaded,'Native point-cloud download timed out');assert.equal(downloaded.suggestedFilename,'chiton-pointcloud.obj');
        const objBytes=fs.readFileSync(path.join(out,downloaded.guid));assert.deepEqual([...crypto.createHash('sha256').update(objBytes).digest()],beforeOBJ.hash);assert.equal(beforeOBJ.units,'normalized_6000_'+cloud.units);const vertices=objBytes.toString('utf8').trim().split('\n');assert.equal(vertices.length,cloud.count);
        assert(vertices.every(line=>/^v -?\d+\.\d{4} -?\d+\.\d{4} -?\d+\.\d{4}$/.test(line)));
        vertices[0].split(' ').slice(1).forEach((n,k)=>assert(Math.abs(Number(n)-cloud.first[k]/6000)<.000051));
        // A real native PNG download contains current style, source/frame provenance and point pixels.
        await browser.evaluate(`window.nativeFooter=[];const text=CanvasRenderingContext2D.prototype.fillText;CanvasRenderingContext2D.prototype.fillText=function(...args){nativeFooter.push(String(args[0]));return text.apply(this,args);};document.getElementById('png-overlays').checked=true;document.getElementById('png').click()`,true);
        let pngEvent;
        for(let i=0;i<200;i++){pngEvent=browser.downloads.filter(e=>e.method==='Browser.downloadWillBegin')[1];if(pngEvent&&browser.downloads.some(e=>e.method==='Browser.downloadProgress'&&e.params.guid===pngEvent.params.guid&&e.params.state==='completed'))break;await browser.sleep(30);}
        assert(pngEvent&&browser.downloads.some(e=>e.method==='Browser.downloadProgress'&&e.params.guid===pngEvent.params.guid&&e.params.state==='completed'));
        assert.equal(browser.downloads.filter(e=>e.method==='Browser.downloadWillBegin').length,2);
        const png=fs.readFileSync(path.join(out,pngEvent.params.guid));assert.equal(png.subarray(0,8).toString('hex'),'89504e470d0a1a0a');fs.writeFileSync(path.join(out,'stub-style-export.png'),png);
        const footer=await browser.evaluate('nativeFooter');assert(footer.some(t=>/CHITON · LIVE API · cloud frame [1-9]/.test(t)));assert(footer.some(t=>/Style: square 7.5 CSS px · perspective · DPR .* · HW /.test(t)));assert(footer.some(t=>/Depth color:/.test(t)));assert(footer.some(t=>/EDL on/.test(t)));assert(footer.some(t=>/orthographic/.test(t)));assert.match(footer.join(' '),/D \[.*X \[.*Y \[.*Z \[/);assert.match(footer.join(' '),/records match clipping/);
        const pngPixels=await browser.evaluate(`(async()=>{const image=await createImageBitmap(new Blob([Uint8Array.from(atob('${png.toString('base64')}'),c=>c.charCodeAt(0))],{type:'image/png'})),canvas=document.createElement('canvas');canvas.width=image.width;canvas.height=image.height;const ctx=canvas.getContext('2d');ctx.drawImage(image,0,0);image.close();const bytes=ctx.getImageData(0,0,canvas.width,Math.max(1,canvas.height-130)).data;let colored=0;for(let i=0;i<bytes.length;i+=4)if(Math.max(...bytes.slice(i,i+3))-Math.min(...bytes.slice(i,i+3))>50)colored++;return {colored,width:canvas.width,height:canvas.height};})()`);assert(pngPixels.colored>100,JSON.stringify(pngPixels));assert.equal(pngPixels.width,png.readUInt32BE(16));assert.equal(pngPixels.height,png.readUInt32BE(20));
        assert.deepEqual(browser.errors,[]);
        fs.writeFileSync(path.join(out,'browser.json'),JSON.stringify({passed:true,highRate,latest:await browser.evaluate('ChitonConsoleApp.state.latest'),cloud,clipping,download:{filename:downloaded.suggestedFilename,vertices:vertices.length},raw:beforeRaw,binary:beforeBinary,png:{footer,...pngPixels},errors:browser.errors},null,2));
        console.log('console_native_browser: actual C++ static handler + live adapter, native JPEG/ROI and actual cloud headers/bytes/WebGL rendering and native OBJ download/vertices/scale passed');
    }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
