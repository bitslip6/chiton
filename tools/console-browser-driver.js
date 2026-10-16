/* Disposable real Chromium/CDP. Loopback origin allowlist; all external traffic blocked. */
'use strict';
const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),http=require('node:http');
const {spawn}=require('node:child_process');
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function browserFor(origin){
    const url=new URL(origin);if(url.protocol!=='http:'||!['127.0.0.1','localhost','[::1]'].includes(url.hostname)||url.username||url.password)throw new Error('Only loopback HTTP targets are permitted');
    origin=url.origin;
    const proxy=http.createServer((req,res)=>{res.writeHead(403);res.end();});proxy.on('connect',(req,socket)=>socket.destroy());
    await new Promise(resolve=>proxy.listen(0,'127.0.0.1',resolve));
    const profile=fs.mkdtempSync(path.join(os.tmpdir(),'chiton-console-'));
    const executable=process.env.CHITON_CHROMIUM||'/usr/bin/chromium';
    if(!fs.existsSync(executable)){proxy.close();fs.rmSync(profile,{recursive:true,force:true});throw new Error('Chromium missing: set CHITON_CHROMIUM to its executable');}
    const browser=spawn(executable,['--headless=new','--no-sandbox','--disable-dev-shm-usage','--no-first-run','--no-default-browser-check',
        '--disable-background-networking','--remote-debugging-pipe','--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader',
        '--user-data-dir='+profile,'--proxy-server=http://127.0.0.1:'+proxy.address().port,'--proxy-bypass-list=127.0.0.1;localhost;[::1]'],{stdio:['ignore','ignore','pipe','pipe','pipe']});
    let next=0,session=null,buffer='',closing=false,stderr='';const pending=new Map(),errors=[],requests=[],failures=[],downloads=[],events=[];
    function command(method,params={},sessionId=session){return new Promise((resolve,reject)=>{
        const id=++next,timer=setTimeout(()=>{pending.delete(id);reject(new Error('CDP timeout: '+method+' '+stderr));},15000);
        pending.set(id,{resolve,reject,timer});browser.stdio[3].write(JSON.stringify({id,method,params,...sessionId?{sessionId}:{}})+'\0');
    });}
    browser.stderr.on('data',data=>stderr=(stderr+data).slice(-4000));
    browser.on('error',error=>{for(const p of pending.values()){clearTimeout(p.timer);p.reject(error);}pending.clear();});
    browser.stdio[4].on('data',data=>{
        buffer+=data;let end;
        while((end=buffer.indexOf('\0'))>=0){const msg=JSON.parse(buffer.slice(0,end));buffer=buffer.slice(end+1);
            if(msg.id){const p=pending.get(msg.id);if(!p)continue;clearTimeout(p.timer);pending.delete(msg.id);msg.error?p.reject(new Error(JSON.stringify(msg.error))):p.resolve(msg.result);}
            else{
                events.push(msg.method);if(events.length>1000)events.shift();
                if(msg.method==='Fetch.requestPaused'){
                    const u=msg.params.request.url,allowed=u.startsWith(origin+'/')||u.startsWith('data:')||u==='about:blank';requests.push(u);if(requests.length>10000)requests.shift();
                    command(allowed?'Fetch.continueRequest':'Fetch.failRequest',{requestId:msg.params.requestId,...allowed?{}:{errorReason:'BlockedByClient'}},msg.sessionId).catch(e=>{if(!closing)errors.push(e.message);});
                }
                if(msg.method==='Runtime.exceptionThrown')errors.push(msg.params.exceptionDetails.exception?.description||msg.params.exceptionDetails.text);
                if(msg.method==='Network.loadingFailed')failures.push(msg.params);
                if(msg.method.startsWith('Browser.download'))downloads.push(msg);
            }
        }
    });
    const target=await command('Target.createTarget',{url:'about:blank'},null);session=(await command('Target.attachToTarget',{targetId:target.targetId,flatten:true},null)).sessionId;
    await command('Runtime.enable');await command('Page.enable');await command('Network.enable');await command('Fetch.enable',{patterns:[{urlPattern:'*'}]});
    async function evaluate(expression,userGesture=false){const r=await command('Runtime.evaluate',{expression,userGesture,awaitPromise:true,returnByValue:true});if(r.exceptionDetails)throw new Error(r.exceptionDetails.exception?.description||r.exceptionDetails.text);return r.result.value;}
    async function wait(expression,timeout=6000){const until=Date.now()+timeout;while(Date.now()<until){if(await evaluate(expression))return;await sleep(30);}throw new Error('Condition timed out: '+expression+'\n'+await evaluate('document.getElementById("notice")?.textContent'));}
    async function close(){closing=true;try{await command('Browser.close',{},null);}catch(_){}await Promise.race([new Promise(resolve=>browser.once('exit',resolve)),sleep(1000)]);if(browser.exitCode==null)browser.kill('SIGKILL');proxy.close();for(const p of pending.values()){clearTimeout(p.timer);p.reject(new Error('Browser closed'));}pending.clear();try{fs.rmSync(profile,{recursive:true,force:true,maxRetries:10,retryDelay:100});}catch(e){console.warn('Profile cleanup:',e.message);}}
    return {command,evaluate,wait,close,errors,requests,failures,downloads,profile,origin,sleep,
        navigate:async url=>{if(new URL(url).origin!==origin)throw new Error('Cross-origin navigation forbidden');await command('Page.navigate',{url});await wait('window.CONSOLE_READY === true');},
        viewport:async(width,height,dpr=1)=>command('Emulation.setDeviceMetricsOverride',{width,height,deviceScaleFactor:dpr,mobile:false}),
        screenshot:async filename=>{const r=await command('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});fs.mkdirSync(path.dirname(filename),{recursive:true});fs.writeFileSync(filename,Buffer.from(r.data,'base64'));}};
}
module.exports={browserFor,sleep};
