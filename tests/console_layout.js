/* Numeric changes must not move readouts, units, adjacent fields, or panels. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture');
const {browserFor}=require('../tools/console-browser-driver');
(async()=>{
    const mock=await fixture(),browser=await browserFor(mock.origin);
    const out=path.resolve('_build/console-review/fixed-width');fs.mkdirSync(out,{recursive:true});
    const reports=[];
    try{
        await browser.navigate(mock.origin+'/console.html?source=demo');
        await browser.evaluate('ChitonConsoleApp.prepareCapture({filter:true})');
        for(const [width,height,dpr] of [[1920,1080,1],[1280,720,1],[1024,768,1],[768,1024,1],[390,844,2],[320,844,1],[640,360,2]]){
            await browser.viewport(width,height,dpr);
            for(const size of ['normal','large','presentation']){
                const report=await browser.evaluate(`(async()=>{
                    const a=ChitonConsoleApp,s=a.state,now=performance.now();
                    document.getElementById('text-size').value=${JSON.stringify(size)};
                    document.getElementById('text-size').dispatchEvent(new Event('change'));
                    document.querySelector('.local-details').open=true;
                    s.filter='Balanced';s.unitConfig='mm';s.segment=1;s.precision=2;s.frozenNow=now;
                    const ids=['measured','units','estimate','estimate-units','mean','stddev','minimum','maximum','range','delta','median','mad','p05','p95','local-sigma','filtered-sigma','measurement-frame','api-count','api-span','local-count','local-span','uncertainty-value','uncertainty-units','measurement-hz','latency','age','image-fps','skipped','measurement','reference','filter','history'];
                    const numeric=ids.filter(id=>!['measurement','reference','filter','history','units','estimate-units','uncertainty-units'].includes(id));
                    const boxes=()=>Object.fromEntries(ids.map(id=>{const e=document.getElementById(id),r=e.getBoundingClientRect();return [id,[r.x+scrollX,r.y+scrollY,r.width,r.height]];}));
                    const samples=[0,1,9,10,99,100,999,1000,9999,10000,65535];let baseline=null;const shifts=[],overflows=[];
                    for(const [index,value] of samples.entries()){
                        s.latest={...s.latest,value,mean:value?Math.max(0,value-.01):0,stddev:value/2,min:0,max:value,range:value,count:index?60:1,frameId:index?9007199254740991:1,firstTime:1700000000000,lastTime:1700000000000+(index?86400000:0),received:now,distinctReceived:now,frameAgeMs:index?999999:0};
                        s.reference=65535;s.estimate=value?value-.01:0;s.latency=index?9999:0;s.skipped=index?9007199254740991:0;
                        s.history.clear();for(let i=0;i<(index?40:1);i++)s.history.push({value:i%2?value:0,estimate:i%2?value-.01:0,segment:1,received:now-(39-i)*50});
                        a.view.render(s,now);
                        if(!index)for(const id of numeric)document.getElementById(id).textContent='—';
                        await new Promise(resolve=>requestAnimationFrame(resolve));
                        const current=boxes();if(!baseline)baseline=current;
                        for(const id of ids)if(current[id].some((v,i)=>Math.abs(v-baseline[id][i])>.1))shifts.push({id,value,before:baseline[id],after:current[id]});
                        // Known uint16 statistics and estimates must fit without wrapping or clipping.
                        for(const id of ['measured','estimate','mean','stddev','minimum','maximum','range','delta','median','mad','p05','p95','local-sigma','filtered-sigma']){
                            const e=document.getElementById(id);if(e.getBoundingClientRect().width && e.scrollWidth>e.clientWidth+1)overflows.push({id,value,scroll:e.scrollWidth,width:e.clientWidth});
                        }
                    }
                    return {shifts,overflows,pageOverflow:document.documentElement.scrollWidth>innerWidth};
                })()`);
                assert.deepEqual(report.shifts,[],`Readout shifted: ${width}px ${size}`);
                assert.deepEqual(report.overflows,[],`Number does not fit: ${width}px ${size}`);
                assert.equal(report.pageOverflow,false,`Page overflow: ${width}px ${size}`);
                reports.push({width,height,dpr,size,...report});
            }
        }
        await browser.viewport(1280,720);await browser.evaluate(`document.getElementById('text-size').value='normal';document.getElementById('text-size').dispatchEvent(new Event('change'));window.scrollTo(0,0);`);
        await browser.screenshot(path.join(out,'desktop-extrema.png'));
        await browser.viewport(390,844);await browser.evaluate(`document.getElementById('measurement').scrollIntoView();`);
        await browser.screenshot(path.join(out,'mobile-extrema.png'));
        assert.deepEqual(browser.errors,[]);
        fs.writeFileSync(path.join(out,'report.json'),JSON.stringify({passed:true,variants:reports},null,2));
        console.log('console_layout: 21 viewport/text-size variants × 11 numeric states; zero geometry shifts, numeric clipping, or page overflow');
    }finally{await browser.close();await mock.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
