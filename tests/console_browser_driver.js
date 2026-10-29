'use strict';
// Exercise launch/exit/setup cleanup without requiring an installed browser.
const assert=require('node:assert/strict');
const fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const {browserFor}=require('../tools/console-browser-driver');

(async()=>{
    const root=fs.mkdtempSync(path.join(os.tmpdir(),'chiton-driver-test-'));
    const original=process.env.CHITON_CHROMIUM;
    const executable=path.join(root,'browser'),record=path.join(root,'record.json');
    function fake(body){
        fs.writeFileSync(executable,`#!${process.execPath}\n`+
            `const fs=require('node:fs');fs.writeFileSync(${JSON.stringify(record)},JSON.stringify({pid:process.pid,profile:process.argv.find(a=>a.startsWith('--user-data-dir=')).split('=')[1]}));\n`+body);
        fs.chmodSync(executable,0o755);
        process.env.CHITON_CHROMIUM=executable;
    }
    function cleaned(){
        const data=JSON.parse(fs.readFileSync(record));
        assert.equal(fs.existsSync(data.profile),false,'temporary profile leaked');
        assert.throws(()=>process.kill(data.pid,0),{code:'ESRCH'},'browser process leaked');
    }
    try{
        process.env.CHITON_CHROMIUM=path.join(root,'missing');
        await assert.rejects(browserFor('http://127.0.0.1:1234'),/Chromium launch failed|ENOENT|EPIPE/);
        fake("process.stderr.write('test startup failure\\n');process.exit(42);\n");
        await assert.rejects(browserFor('http://127.0.0.1:1234'),/Chromium startup failed.*test startup failure/s);
        cleaned();
        fake(`const input=fs.createReadStream(null,{fd:3});let buffer='';input.on('data',chunk=>{buffer+=chunk;let end;while((end=buffer.indexOf('\\0'))>=0){const msg=JSON.parse(buffer.slice(0,end));buffer=buffer.slice(end+1);fs.writeSync(4,JSON.stringify({id:msg.id,error:{message:'setup rejected'}})+'\\0');}});`);
        await assert.rejects(browserFor('http://127.0.0.1:1234'),/setup rejected/);
        cleaned();
        fake(`const input=fs.createReadStream(null,{fd:3});let buffer='';input.on('data',chunk=>{buffer+=chunk;let end;while((end=buffer.indexOf('\\0'))>=0){const msg=JSON.parse(buffer.slice(0,end));buffer=buffer.slice(end+1);fs.writeSync(4,JSON.stringify({id:msg.id,result:{targetId:'target',sessionId:'session'}})+'\\0');if(msg.method==='Browser.close')process.exit(0);}});`);
        const browser=await browserFor('http://127.0.0.1:1234');
        await browser.close();await browser.close();
        cleaned();
        console.log('Browser driver launch, exit, setup failure and cleanup checks passed');
    }finally{
        if(original===undefined)delete process.env.CHITON_CHROMIUM;else process.env.CHITON_CHROMIUM=original;
        fs.rmSync(root,{recursive:true,force:true});
    }
})().catch(error=>{console.error(error);process.exitCode=1;});
