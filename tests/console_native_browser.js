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
        assert.deepEqual(browser.errors,[]);
        fs.writeFileSync(path.join(out,'browser.json'),JSON.stringify({passed:true,latest:await browser.evaluate('ChitonConsoleApp.state.latest'),errors:browser.errors},null,2));
        console.log('console_native_browser: actual C++ static handler + live adapter, native JPEG and ROI passed');
    }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
