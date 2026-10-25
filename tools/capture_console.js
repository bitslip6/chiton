#!/usr/bin/env node
'use strict';
const fs=require('node:fs'),path=require('node:path');
const {browserFor}=require('./console-browser-driver');
async function capture({url='http://127.0.0.1:8765/console.html',out='_build/console-review/latest',matrix:matrixName='2d',pointStyle,clipSettings}={}){
    if(!['2d','3d','3d-style','3d-clipping','display-first'].includes(matrixName))throw new Error('Unknown capture matrix');
    if(pointStyle!==undefined)pointStyle=require('../release/files/console-cloud-material').validatePointStyle(pointStyle);
    if(clipSettings!==undefined)clipSettings=require('../release/files/console-cloud').validateClipSettings(clipSettings);
    const target=new URL(url);if(target.searchParams.get('source')==='live')throw new Error('Screenshot automation only freezes demo sources');
    const browser=await browserFor(target.origin),manifest={browser:await browser.command('Browser.getVersion',{},null),captures:[]};
    const matrix=[[1920,1080],[1440,900],[1280,720],[1024,768],[768,1024],[390,844]].map(([width,height])=>({name:`stable-${width}x${height}`,width,height,scenario:'stable'}));
    matrix.push(...[
        {name:'roi',scenario:'roi'},{name:'filter',scenario:'stable',filter:true},{name:'settings',scenario:'stable',settings:true},
        {name:'frozen-stale',scenario:'frozen'},{name:'disconnected',scenario:'restart',timeMs:9000},{name:'warming',scenario:'warming'},
        {name:'ir',scenario:'ir'},{name:'panorama',scenario:'panorama'},{name:'long-values',scenario:'long-values'},
        {name:'large-text',scenario:'stable',size:'large'},{name:'presentation',scenario:'stable',size:'presentation'},
        {name:'zoom-200',scenario:'stable',width:640,height:360,dpr:2,zoomEquivalent:200}
    ].map(v=>({width:1280,height:720,...v})));
    if(matrixName==='3d'){
        matrix.length=0;
        for(const [width,height] of [[1920,1080],[1440,900],[1280,720],[1024,768],[768,1024],[390,844]])matrix.push({name:`cloud-${width}x${height}`,width,height,scenario:'cloud'});
        matrix.push(...[{name:'cloud-rotated',pose:'rotated'},{name:'cloud-inspector',filter:true},{name:'cloud-empty',scenario:'cloud-empty'},
            {name:'cloud-stale',scenario:'frozen'},{name:'cloud-ir-unavailable',ir:true},{name:'cloud-webgl-unavailable',noWebGL:true},
            {name:'cloud-settings',settings:true},{name:'cloud-large',size:'large'},{name:'cloud-presentation',size:'presentation'},
            {name:'cloud-zoom-200',width:640,height:360,dpr:2,zoomEquivalent:200}].map(v=>({width:1280,height:720,scenario:'cloud',...v})));
    }
    if(matrixName==='3d-style'){
        matrix.length=0;
        for(const [width,height] of [[1920,1080],[1280,720],[390,844],[320,844],[640,360]])for(const shape of ['square','circle'])matrix.push({name:`${shape}-2.5-${width}x${height}`,width,height,scenario:'cloud',expanded:true,pointStyle:{shape,diameterCssPx:2.5,sizing:'screen'},...(width===640?{dpr:2,size:'presentation',zoomEquivalent:200}:{})});
        for(const shape of ['square','circle'])for(const diameterCssPx of [1,10])for(const sizing of ['screen','perspective'])matrix.push({name:`${shape}-${diameterCssPx}-${sizing}`,width:1280,height:720,scenario:'cloud',pointStyle:{shape,diameterCssPx,sizing},expanded:true});
        for(const shape of ['circle','square'])matrix.push({name:`reflow-controls-${shape}`,width:640,height:360,dpr:2,size:'presentation',zoomEquivalent:200,scenario:'cloud',expanded:true,appearanceFocus:true,pointStyle:{shape,diameterCssPx:2.5,sizing:'screen'}});
        for(const width of [1280,320])matrix.push({name:`fullscreen-${width}`,width,height:width===320?844:720,scenario:'cloud',pointStyle:{shape:'circle',diameterCssPx:7.5,sizing:'perspective'},expanded:true,fullscreen:true});
    }
    if(matrixName==='3d-clipping'){
        matrix.length=0;const C=require('../release/files/console-cloud'),off=C.defaultClipSettings(),depth=C.defaultClipSettings(),box=C.defaultClipSettings(),zero=C.defaultClipSettings();
        depth.depth={enabled:true,min:900,max:1800};box.box={enabled:true,min:[-800,-900,-1000],max:[900,800,2500]};zero.depth={enabled:true,min:-32768,max:-32768};
        const intersection={depth:depth.depth,box:box.box};
        for(const projection of ['perspective','orthographic'])for(const eyeDomeLighting of [false,true])for(const [name,clipSettings] of Object.entries({off,depth,box,intersection,zero}))matrix.push({name:`${name}-${projection}-${eyeDomeLighting?'edl':'plain'}`,scenario:'cloud',width:1280,height:720,projection,eyeDomeLighting,clipSettings,clippingExpanded:true});
        for(const [width,fullscreen] of [[390,false],[320,false],[320,true]])matrix.push({name:`clipping-${width}-${fullscreen?'fullscreen':'mobile'}`,scenario:'cloud',width,height:844,clipSettings:intersection,clippingExpanded:true,fullscreen});
    }
    if(matrixName==='display-first'){
        matrix.length=0;const C=require('../release/files/console-cloud'),clip=C.defaultClipSettings();clip.depth={enabled:true,min:900,max:1800};
        for(const [width,height] of [[1920,1080],[1280,720],[390,844],[320,844],[640,360]])for(const controls of [false,true])matrix.push({name:`display-${width}-${controls?'open':'closed'}`,width,height,scenario:'cloud',controls,clipSettings:clip,clippingExpanded:controls});
        for(const entry of [{name:'invalid-draft',invalid:true,controls:true,clippingExpanded:true,clipSettings:clip},{name:'zero-match',clipSettings:{...clip,depth:{enabled:true,min:-32768,max:-32768}}},{name:'fullscreen-open',fullscreen:true,controls:true},{name:'fullscreen-closed',fullscreen:true},{name:'rate-30',controls:true,rate:30},{name:'panorama',scenario:'panorama',cloudMode:false},{name:'image',scenario:'stable',cloudMode:false},{name:'cloud-stale',scenario:'frozen'},{name:'cloud-unavailable',noWebGL:true}])matrix.push({width:1280,height:720,scenario:'cloud',...entry});
    }
    try{
        for(const entry of matrix){
            await browser.viewport(entry.width,entry.height,entry.dpr||1);
            target.searchParams.set('source','demo');target.searchParams.set('scenario',entry.scenario);target.searchParams.set('seed','7');
            let injection=null;
            if(entry.noWebGL)injection=await browser.command('Page.addScriptToEvaluateOnNewDocument',{source:`const original=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(type,...args){return type.includes('webgl')?null:original.call(this,type,...args);};`});
            await browser.navigate(target.href);
            if(injection)await browser.command('Page.removeScriptToEvaluateOnNewDocument',{identifier:injection.identifier});
            const evidence=await browser.evaluate(`ChitonConsoleApp.prepareCapture(${JSON.stringify({filter:!!entry.filter,timeMs:entry.timeMs??6000,cloudMode:entry.cloudMode??matrixName!=='2d',cloudIR:!!entry.ir,...((entry.clipSettings||clipSettings)?{clipSettings:entry.clipSettings||clipSettings}:{}),...(entry.projection?{projection:entry.projection}:{}),...(typeof entry.eyeDomeLighting==='boolean'?{eyeDomeLighting:entry.eyeDomeLighting}:{}),...((entry.pointStyle||pointStyle)?{pointStyle:entry.pointStyle||pointStyle}:{})})})`);
            if(entry.pose)await browser.evaluate('ChitonConsoleApp.view.cloud.nav.orbit(.6,-.2);ChitonConsoleApp.view.cloud.nav.pan(30,10);ChitonConsoleApp.view.cloud.render()');
            await browser.evaluate(`document.getElementById('text-size').value=${JSON.stringify(entry.size||'normal')};document.getElementById('text-size').dispatchEvent(new Event('change'));`);
            if(entry.controls||entry.expanded||entry.clippingExpanded||entry.settings)await browser.evaluate('if(document.getElementById("display-controls")?.hidden)document.getElementById("toggle-display-controls").click()');
            if(entry.settings)await browser.evaluate('document.getElementById("display-help").open=true;document.getElementById("open-settings").focus();document.getElementById("open-settings").click()');
            if(entry.rate)await browser.evaluate(`document.getElementById('display-refresh').open=true;document.getElementById('cloud-rate').value='${entry.rate}';document.getElementById('cloud-rate').dispatchEvent(new Event('change'));document.getElementById('cloud-rate').scrollIntoView({block:'center'})`);
            if(entry.invalid)await browser.evaluate(`document.getElementById('clip-d-min').value='';document.getElementById('clip-d-min').dispatchEvent(new Event('input',{bubbles:true}))`);
            if(entry.clippingExpanded)await browser.evaluate('document.getElementById("clip-details").open=true');
            if(entry.expanded)await browser.evaluate('document.getElementById("point-appearance").open=true');
            if(entry.appearanceFocus)await browser.evaluate('document.getElementById("point-appearance").scrollIntoView({block:"center"})');
            if(entry.fullscreen){await browser.evaluate('document.getElementById("fullscreen").click()',true);await browser.wait('document.fullscreenElement?.id==="viewport"');}
            await browser.evaluate('new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))');
            const layout=await browser.evaluate(`({innerWidth,scrollWidth:document.documentElement.scrollWidth,height:innerHeight,overflow:document.documentElement.scrollWidth>innerWidth,source:document.getElementById('source-badge').textContent,imageState:document.getElementById('image-state').textContent,measurementState:document.getElementById('measurement-state').textContent,panelOpen:!document.getElementById('display-controls')?.hidden,rects:Object.fromEntries(['viewport','measurement','display-controls'].map(id=>[id,document.getElementById(id)?.getBoundingClientRect().toJSON()])),requestedCloudHz:ChitonConsoleApp.state.cloudHz,distinctCloudHz:ChitonConsoleApp.state.cloud.hz(ChitonConsoleApp.state.frozenNow??performance.now())})`);
            const filename=path.resolve(out,entry.name+'.png');await browser.screenshot(filename);
            const renderer=await browser.evaluate('ChitonConsoleApp.view.cloud?.counters()||null');
            const cloud=evidence.cloud?{...evidence.cloud,frameId:evidence.cloud.frameId??null,recordCount:evidence.cloud.recordCount??null,renderer}:null;
            let png=null;
            if(['3d-style','3d-clipping','display-first'].includes(matrixName)&&renderer?.points){const base64=await browser.evaluate(`(async()=>{const a=ChitonConsoleApp,blob=await a.view.cloud.png(a.state.cloud.latest,{source:'demo',stale:false,overlays:true});return await new Promise(resolve=>{const r=new FileReader();r.onload=()=>resolve(r.result.split(',')[1]);r.readAsDataURL(blob);});})()`),bytes=Buffer.from(base64,'base64'),exportFilename=path.resolve(out,entry.name+'-export.png');fs.writeFileSync(exportFilename,bytes);png={filename:exportFilename,bytes:bytes.length,width:bytes.readUInt32BE(16),height:bytes.readUInt32BE(20)};}
            manifest.captures.push({...entry,...evidence,layout,filename,cloud,png,dpr:entry.dpr||1,renderReady:true});
            if(layout.overflow)throw new Error('Horizontal overflow: '+entry.name);
            if(['frozen-stale','disconnected','cloud-stale'].includes(entry.name) && (!layout.imageState.includes('stale')||!layout.measurementState.includes('stale')))throw new Error('Fault scenario falsely appears fresh: '+entry.name);
        }
        manifest.errors=[...browser.errors];manifest.failedRequests=browser.failures;manifest.externalRequests=browser.requests.filter(v=>!v.startsWith(target.origin+'/')&&!v.startsWith('data:'));
        if(manifest.errors.length||manifest.externalRequests.length)throw new Error('Browser errors or external traffic: '+JSON.stringify(manifest.errors));
    }finally{fs.mkdirSync(out,{recursive:true});fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify(manifest,null,2));await browser.close();}
    console.log('Captured '+manifest.captures.length+' views: '+out);return manifest;
}
if(require.main===module){const args=process.argv.slice(2),options={};for(let i=0;i<args.length;i+=2){if(!['--url','--out','--matrix','--point-style','--clip-settings'].includes(args[i])||!args[i+1])throw new Error('Usage: node tools/capture_console.js [--url loopback-url] [--out directory] [--matrix 2d|3d|3d-style|3d-clipping|display-first] [--point-style complete-json] [--clip-settings complete-json]');if(args[i]==='--point-style')options.pointStyle=JSON.parse(args[i+1]);else if(args[i]==='--clip-settings')options.clipSettings=JSON.parse(args[i+1]);else options[args[i].slice(2)]=args[i+1];}capture(options).catch(e=>{console.error(e);process.exitCode=1;});}
module.exports={capture};
