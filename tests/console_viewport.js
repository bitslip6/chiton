/* Viewer-first layout: no masthead/banner row, full-window panel, usable controls. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
(async()=>{
    const mock=await fixture(),b=await browserFor(mock.origin),out=path.resolve('_build/console-viewer-layout-review');fs.mkdirSync(out,{recursive:true});
    const variants=[],app='ChitonConsoleApp',click=id=>b.evaluate(`document.getElementById(${JSON.stringify(id)}).click()`,true);
    try{
        for(const scenario of ['stable','cloud']){
            await b.viewport(1280,720);await b.navigate(mock.origin+'/console.html?source=demo&scenario='+scenario);
            await b.evaluate(`${app}.prepareCapture()`);
            for(const [width,height,dpr] of [[1920,1080,1],[1440,900,1],[1280,720,1],[1024,768,1],[768,1024,1],[390,844,2],[320,844,1],[640,360,2]]){
                await b.viewport(width,height,dpr);
                for(const size of ['normal','large','presentation']){
                    const report=await b.evaluate(`(async()=>{
                        document.getElementById('text-size').value=${JSON.stringify(size)};document.getElementById('text-size').dispatchEvent(new Event('change'));
                        document.getElementById('show-overlay').checked=false;document.getElementById('show-overlay').dispatchEvent(new Event('change'));window.scrollTo(0,0);
                        await new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)));
                        const panel=document.querySelector('.sensor-panel'),toolbar=panel.querySelector('.toolbar'),p=panel.getBoundingClientRect(),t=toolbar.getBoundingClientRect(),v=document.getElementById('viewport').getBoundingClientRect();
                        const clipped=[...toolbar.querySelectorAll('button,select'),...panel.querySelectorAll('.selection-bar button,.cloud-navigation>button,.cloud-navigation select')].filter(e=>e.getBoundingClientRect().width).filter(e=>{const r=e.getBoundingClientRect();return r.left<p.left||r.right>p.right||r.top<p.top||r.bottom>p.bottom;}).map(e=>e.id);
                        const darkBackgrounds=[...document.querySelectorAll('.viewport-top,.viewport-bottom,.viewport-top span,.viewport-bottom span,#cloud-legend')].filter(e=>{const css=getComputedStyle(e);return css.backgroundColor!=='rgba(0, 0, 0, 0)'||css.backgroundImage!=='none';}).map(e=>e.id||e.className);
                        return {top:p.top,height:p.height,viewportHeight:v.height,toolbarTop:t.top,clipped,darkBackgrounds,pageOverflow:document.documentElement.scrollWidth>innerWidth,
                            masthead:!!document.querySelector('.masthead,.brand,img[src="/jabil.png"]'),fit:!!document.getElementById('fit'),noticeHidden:document.getElementById('notice').hidden,
                            controlsMoved:['pause-viewer','open-settings','source-badge'].every(id=>toolbar.contains(document.getElementById(id))),demo:document.getElementById('viewport-source').textContent};
                    })()`);
                    assert.deepEqual(report.darkBackgrounds,[],`${scenario} ${width} ${size}: opaque label backing`);
                    assert.equal(report.masthead,false);assert.equal(report.fit,false);assert.equal(report.noticeHidden,true);assert.equal(report.controlsMoved,true);assert.match(report.demo,/DEMO.*SYNTHETIC/);
                    assert.equal(report.pageOverflow,false,`${scenario} ${width} ${size}`);assert.deepEqual(report.clipped,[],`${scenario} ${width} ${size}`);assert(Math.abs(report.top)<1);assert(Math.abs(report.toolbarTop)<=1.01);assert(report.viewportHeight>=180);
                    assert(report.height>=height-1);if(height>=720)assert(Math.abs(report.height-height)<1,JSON.stringify({width,height,size,scenario,...report}));
                    variants.push({scenario,width,height,dpr,size,...report});
                    if(size==='normal'&&[1920,1280,390,640].includes(width))await b.screenshot(path.join(out,`${scenario}-${width}x${height}.png`));
                }
            }
            if(scenario==='stable'){
                const caption=await b.evaluate(`(()=>{const a=ChitonConsoleApp;document.getElementById('show-overlay').checked=true;a.view.drawOverlay({...a.state,selection:{kind:'point',x:460,y:320}});const ctx=a.view.overlay.getContext('2d'),pixels=ctx.getImageData(476,288,90,24).data;return {backingAlpha:ctx.getImageData(478,296,1,1).data[3],hasText:pixels.some((v,i)=>i%4===3&&v>0)};})()`);
                assert.equal(caption.backingAlpha,0,'Sensor-coordinate label must not fill a dark rectangle');assert.equal(caption.hasText,true);
            }
            // Settings help is allowed to grow the panel on short/narrow screens, never clip controls.
            await b.viewport(320,360);await b.evaluate(`document.querySelector('#cloud-navigation details').open=true`);
            const visible=await b.evaluate(`(()=>{const p=document.querySelector('.sensor-panel').getBoundingClientRect(),r=document.querySelector('.selection-bar').getBoundingClientRect();return r.bottom<=p.bottom;})()`);assert.equal(visible,true);
            await b.viewport(1280,720);await b.evaluate(`document.querySelector('#cloud-navigation details').open=false;window.scrollTo(0,0)`);
            await click('open-settings');assert.equal(await b.evaluate('document.getElementById("settings").open'),true);await click('close-settings');await b.wait('document.activeElement.id==="open-settings"');
            await click('pause-viewer');assert.equal(await b.evaluate(`${app}.state.paused`),true);await click('pause-viewer');assert.equal(await b.evaluate(`${app}.state.paused`),false);
        }
        // The normal live banner is gone, but real errors remain visible without resizing the view.
        await b.navigate(mock.origin+'/console.html?source=live');await b.wait(`${app}.state.latest!=null && ${app}.state.device.mode===1`);
        assert.equal(await b.evaluate('document.getElementById("notice").hidden'),true);assert.equal(mock.source.mutations.length,0);
        const height=await b.evaluate('document.querySelector(".sensor-panel").getBoundingClientRect().height');mock.control.measurement='503';
        await b.wait(`!document.getElementById('notice').hidden && document.getElementById('notice').textContent.includes('Measurement delayed')`);
        assert.equal(await b.evaluate('document.querySelector(".sensor-panel").getBoundingClientRect().height'),height);assert.equal(await b.evaluate('document.getElementById("notice").closest(".viewport")!=null'),true);
        mock.control.measurement='ok';await b.wait(`${app}.state.phase==='live' && document.getElementById('notice').hidden`);
        await b.command('Page.navigate',{url:mock.origin+'/console.html?source=invalid'});await b.wait('document.getElementById("notice").classList.contains("fatal") && !document.getElementById("notice").hidden');
        assert.deepEqual(b.errors,[]);assert.equal(b.requests.some(url=>!url.startsWith(mock.origin+'/')&&!url.startsWith('data:')),false);
        fs.writeFileSync(path.join(out,'viewport-report.json'),JSON.stringify({passed:true,variants,errors:b.errors},null,2));
        console.log('console_viewport: 48 2D/3D size/reflow variants, full-window viewer, moved settings/pause, no masthead/Fit/normal banner or dark label backing, transparent coordinate caption, visible errors/demo, unclipped controls passed');
    }finally{await b.close();await mock.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
