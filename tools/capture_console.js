#!/usr/bin/env node
'use strict';
const fs=require('node:fs'),path=require('node:path');
const {browserFor}=require('./console-browser-driver');
async function capture({url='http://127.0.0.1:8765/console.html',out='_build/console-review/latest'}={}){
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
    try{
        for(const entry of matrix){
            await browser.viewport(entry.width,entry.height,entry.dpr||1);
            target.searchParams.set('source','demo');target.searchParams.set('scenario',entry.scenario);target.searchParams.set('seed','7');
            await browser.navigate(target.href);
            const evidence=await browser.evaluate(`ChitonConsoleApp.prepareCapture(${JSON.stringify({filter:!!entry.filter,timeMs:entry.timeMs??6000})})`);
            await browser.evaluate(`document.getElementById('text-size').value=${JSON.stringify(entry.size||'normal')};document.getElementById('text-size').dispatchEvent(new Event('change'));`);
            if(entry.settings)await browser.evaluate('document.getElementById("open-settings").click()');
            await browser.evaluate('new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))');
            const layout=await browser.evaluate(`({innerWidth,scrollWidth:document.documentElement.scrollWidth,height:innerHeight,overflow:document.documentElement.scrollWidth>innerWidth,source:document.getElementById('source-badge').textContent,imageState:document.getElementById('image-state').textContent,measurementState:document.getElementById('measurement-state').textContent})`);
            const filename=path.resolve(out,entry.name+'.png');await browser.screenshot(filename);
            manifest.captures.push({...entry,...evidence,layout,filename});
            if(layout.overflow)throw new Error('Horizontal overflow: '+entry.name);
            if(['frozen-stale','disconnected'].includes(entry.name) && (!layout.imageState.includes('stale')||!layout.measurementState.includes('stale')))throw new Error('Fault scenario falsely appears fresh: '+entry.name);
        }
        manifest.errors=[...browser.errors];manifest.failedRequests=browser.failures;manifest.externalRequests=browser.requests.filter(v=>!v.startsWith(target.origin+'/')&&!v.startsWith('data:'));
        if(manifest.errors.length||manifest.externalRequests.length)throw new Error('Browser errors or external traffic: '+JSON.stringify(manifest.errors));
    }finally{fs.mkdirSync(out,{recursive:true});fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify(manifest,null,2));await browser.close();}
    console.log('Captured '+manifest.captures.length+' views: '+out);return manifest;
}
if(require.main===module){const args=process.argv.slice(2),options={};for(let i=0;i<args.length;i+=2){if(!['--url','--out'].includes(args[i])||!args[i+1])throw new Error('Usage: node tools/capture_console.js [--url loopback-url] [--out directory]');options[args[i].slice(2)]=args[i+1];}capture(options).catch(e=>{console.error(e);process.exitCode=1;});}
module.exports={capture};
