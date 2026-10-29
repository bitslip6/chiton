/* Actual display area, stable overlay bounds, retained sidebar and responsive order. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {fixture}=require('./console_fixture'),{browserFor}=require('../tools/console-browser-driver');
(async()=>{
 const f=await fixture(),b=await browserFor(f.origin),out=path.resolve('_build/display-first-ui/validation');fs.mkdirSync(out,{recursive:true});const variants=[];
 try{
  for(const scenario of ['stable','cloud']){
   await b.viewport(1280,720);await b.navigate(f.origin+'/console.html?source=demo&scenario='+scenario);await b.evaluate('ChitonConsoleApp.prepareCapture()');
   for(const [width,height] of [[1920,1080],[1440,900],[1280,720],[1024,768],[768,1024],[390,844],[320,844],[640,360]])for(const dpr of [1,2,3])for(const size of ['normal','large','presentation']){
    await b.viewport(width,height,dpr);
    const result=await b.evaluate(`(async()=>{
     const $=id=>document.getElementById(id),a=ChitonConsoleApp;document.body.dataset.size='${size}';window.scrollTo(0,0);
     if(!$('display-controls').hidden)$('toggle-display-controls').click();
     const settle=()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)));await settle();
     const rect=id=>$(id).getBoundingClientRect().toJSON(),state=()=>({rect:rect('viewport'),buffer:a.view.cloud?[a.view.cloud.canvas.width,a.view.cloud.canvas.height]:[a.view.sensor.width,a.view.sensor.height],pose:a.view.cloud?.nav.pose(),reference:a.view.cloud?.referenceDistance,scroll:scrollY});
     const closed=state(),sidebar=rect('measurement');$('toggle-display-controls').click();$('clip-details').open=true;document.querySelector('#display-help details').open=true;$('display-help').open=true;await settle();
     const open=state(),panel=rect('display-controls'),close=rect('close-display-controls');
     const panelOverflow=$('display-controls').scrollWidth>$('display-controls').clientWidth;
     $('close-display-controls').click();await settle();
     return {closed,open,after:state(),sidebar,panel,close,panelOverflow,pageOverflow:document.documentElement.scrollWidth>innerWidth,inspectorOverflow:getComputedStyle($('measurement')).overflowY,source:$('viewport-source').textContent,noticeHidden:$('notice').hidden,order:[...document.querySelectorAll('.sensor-panel,#measurement,.history')].map(e=>e.id||e.className)};
    })()`);
    assert.equal(result.pageOverflow,false,JSON.stringify({width,height,dpr,size,result}));assert.equal(result.panelOverflow,false);
    assert(result.closed.rect.height>=height*.9);assert(result.closed.rect.x<=16);assert(result.closed.rect.width>0);
    for(const state of [result.open,result.after]){assert.deepEqual(state.buffer,result.closed.buffer);assert.deepEqual(state.pose,result.closed.pose);assert.equal(state.reference,result.closed.reference);assert.equal(state.scroll,result.closed.scroll);for(const key of ['x','y','width','height'])assert(Math.abs(state.rect[key]-result.closed.rect[key])<=1);}
    assert(result.panel.x>=result.open.rect.x&&result.panel.right<=result.open.rect.right);assert(result.panel.bottom<=height);assert(result.close.y>=0&&result.close.bottom<=height);
    if(width>=900){assert(result.sidebar.x>=result.closed.rect.right);assert.equal(result.inspectorOverflow,'auto');assert(result.sidebar.height<=height+1);}else {assert(result.sidebar.y>=height-1);assert.equal(result.inspectorOverflow,'visible');assert.match(result.order[1],/measurement/);}
    assert.match(result.source,/DEMO.*SYNTHETIC/);assert.equal(result.noticeHidden,true);variants.push({scenario,width,height,dpr,size,...result});
   }
  }
  await b.viewport(1280,720);await b.navigate(f.origin+'/console.html');await b.wait('ChitonConsoleApp.state.latest!=null');const height=await b.evaluate('document.getElementById("viewport").clientHeight');f.control.measurement='503';await b.wait('document.getElementById("notice").textContent.includes("Measurement delayed")');assert.equal(await b.evaluate('document.getElementById("viewport").clientHeight'),height);assert.equal(f.source.mutations.length,0);
  assert.deepEqual(b.errors,[]);fs.writeFileSync(path.join(out,'viewport.json'),JSON.stringify({passed:true,variants},null,2));console.log('console_viewport: '+variants.length+' display/sidebar/text/DPR variants; >=90% actual canvas height, stable open/closed bounds/buffers/pose, no overflow or scroll jump');
 }finally{await b.close();await f.close();}
})().catch(e=>{console.error(e);process.exitCode=1});
