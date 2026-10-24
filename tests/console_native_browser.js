/* Invoked by console_native.py only after identifying the isolated SDK-free stub. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {browserFor}=require('../tools/console-browser-driver');
(async()=>{
    const origin=process.argv[2];if(!origin)throw new Error('Use python3 tests/console_native.py /path/to/stub');
    const browser=await browserFor(origin);
    try{
        await browser.viewport(1280,720);await browser.navigate(origin+'/console.html?source=live');
        await browser.wait('ChitonConsoleApp.state.latest?.value===800 && ChitonConsoleApp.view.lastImage!=null',15000);
        assert.equal(await browser.evaluate('ChitonConsoleApp.state.latest.units'),'depth_sensor_units');
        await browser.evaluate('ChitonConsoleApp.setSelection({kind:"roi",x:450,y:300,x2:470,y2:330})');
        await browser.wait('ChitonConsoleApp.state.latest?.statisticsKind==="spatial"');
        assert.equal(await browser.evaluate('ChitonConsoleApp.state.latest.count'),600);
        const out=path.resolve('_build/console-review/native');fs.mkdirSync(out,{recursive:true});await browser.screenshot(path.join(out,'stub-live.png'));
        await browser.evaluate(`ChitonConsoleApp.source.applySetting({name:'mode',value:4})`);
        await browser.wait('ChitonConsoleApp.state.device.mode===4 && ChitonConsoleApp.view.cloud?.count>0',15000);
        assert.equal(await browser.evaluate('ChitonConsoleApp.state.selection.kind'),'point');
        const cloud=await browser.evaluate(`(async()=>{const a=ChitonConsoleApp,m=await a.source.readCloud();return {frame:m.frameId,count:m.recordCount,bytes:m.bytes,units:m.units,scope:m.unitsScope,first:[...m.records.slice(0,4)],rendered:a.view.cloud.count,positions:[...a.view.cloud.positions.slice(0,3)],resources:a.view.cloud.counters()};})()`);
        assert(cloud.count>0&&cloud.count<=640*480);assert.equal(cloud.bytes,cloud.count*8);assert.equal(cloud.rendered,cloud.count);assert.deepEqual(cloud.positions,cloud.first.slice(0,3));assert.equal(cloud.units,'depth_sensor_units');
        await browser.screenshot(path.join(out,'stub-cloud.png'));
        await browser.command('Browser.setDownloadBehavior',{behavior:'allowAndName',downloadPath:out,eventsEnabled:true},null);
        assert.equal(await browser.evaluate('!document.getElementById("obj").disabled && document.getElementById("obj").getBoundingClientRect().width>0'),true);
        await browser.evaluate('ChitonConsoleApp.view.cloud.nav.orbit(.5,.2);document.getElementById("obj").click()',true);
        let downloaded=null;
        for(let i=0;i<200;i++){
            const starts=browser.downloads.filter(e=>e.method==='Browser.downloadWillBegin'),event=starts[0];
            if(event&&browser.downloads.some(e=>e.method==='Browser.downloadProgress'&&e.params.guid===event.params.guid&&e.params.state==='completed')){assert.equal(starts.length,1);downloaded=event.params;break;}
            await browser.sleep(30);
        }
        assert(downloaded,'Native point-cloud download timed out');assert.equal(downloaded.suggestedFilename,'chiton-pointcloud.obj');
        const vertices=fs.readFileSync(path.join(out,downloaded.guid),'utf8').trim().split('\n');assert.equal(vertices.length,cloud.count);
        assert(vertices.every(line=>/^v -?\d+\.\d{4} -?\d+\.\d{4} -?\d+\.\d{4}$/.test(line)));
        vertices[0].split(' ').slice(1).forEach((n,k)=>assert(Math.abs(Number(n)-cloud.first[k]/6000)<.000051));
        assert.deepEqual(browser.errors,[]);
        fs.writeFileSync(path.join(out,'browser.json'),JSON.stringify({passed:true,latest:await browser.evaluate('ChitonConsoleApp.state.latest'),cloud,download:{filename:downloaded.suggestedFilename,vertices:vertices.length},errors:browser.errors},null,2));
        console.log('console_native_browser: actual C++ static handler + live adapter, native JPEG/ROI and actual cloud headers/bytes/WebGL rendering and native OBJ download/vertices/scale passed');
    }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
