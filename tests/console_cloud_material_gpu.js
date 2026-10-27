/* Actual bundled r128 + signed SHORT framebuffer qualification. Loopback/disposable Chromium only. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),http=require('node:http'),path=require('node:path');
const {browserFor}=require('../tools/console-browser-driver');
async function framebufferChecks(){
    const C=window.ChitonConsole,T=window.THREE;
    const check=(ok,message)=>{if(!ok)throw new Error(message);};
    const same=(a,b,message)=>check(JSON.stringify(a)===JSON.stringify(b),message+': '+JSON.stringify([a,b]));
    const host=document.getElementById('host'),view=new C.CloudView(host);window.testView=view;
    check(view.material.depthTest&&view.material.depthWrite&&!view.material.transparent&&view.material.blending===T.NoBlending,'opaque depth-tested material');
    same(view.pointStyle,{shape:'circle',diameterCssPx:2.5,sizing:'screen'},'new view defaults');
    // Disable autonomous paints while explicitly exercising the real renderer synchronously.
    if(view.raf!=null)cancelAnimationFrame(view.raf);view.raf=null;view.dirty=()=>{};view.axes.visible=false;
    const gl=view.renderer.getContext(),uploads=[],bindings=[];
    for(const method of ['bufferData','bufferSubData']){const original=gl[method].bind(gl);gl[method]=(...args)=>{
        if(args[0]===gl.ARRAY_BUFFER){const data=args[method==='bufferData'?1:2];uploads.push({method,bytes:typeof data==='number'?data:(args[4]||data.length)*data.BYTES_PER_ELEMENT});}
        return original(...args);
    };}
    const pointer=gl.vertexAttribPointer.bind(gl);gl.vertexAttribPointer=(...args)=>{bindings.push(args);return pointer(...args);};
    const target=new T.WebGLRenderTarget(512,512,{minFilter:T.NearestFilter,magFilter:T.NearestFilter});
    const camera=new T.OrthographicCamera(-40000,40000,40000,-40000,.1,200000);camera.position.set(0,0,100000);camera.lookAt(0,0,0);camera.updateMatrixWorld();
    const clear=[37,53,71];view.renderer.setClearColor(new T.Color(...clear.map(v=>v/255)));
    const snapshot=records=>({records:new Int16Array(records),recordCount:records.length/4,frameId:7,source:'test',revision:1,frameType:'depth',units:'mm',unitsScope:'calibration_aoi'});
    const style=(shape='square',diameterCssPx=10,sizing='screen')=>view.setPointStyle({shape,diameterCssPx,sizing});
    function draw(){view.renderer.setRenderTarget(target);view.renderer.render(view.scene,camera);view.checkPrograms();}
    function pixel(x,y){const p=new Uint8Array(4);view.renderer.readRenderTargetPixels(target,x,y,1,1,p);return [...p.slice(0,3)];}
    function at(x,y,z=0){const p=new T.Vector3(x,y,z).project(camera);return [Math.floor((p.x+1)*256),Math.floor((p.y+1)*256)];}
    function colorAt(x,y,z,expected,label){const got=pixel(...at(x,y,z));check(got.every((v,k)=>Math.abs(v-expected[k]*255)<=2),label+': '+JSON.stringify({got,expected}));return got;}
    style();
    const depths=[-32768,-1,0,125,250,375,500,750,1000,1100,32767],records=[];
    depths.forEach((d,i)=>records.push(i===depths.length-1?32767:-32768+i*6000,i%2?-32768:32767,i%3===0?-32768:i%3===1?32767:0,d));
    const source=snapshot(records),before=[...source.records];view.update(source,{min:0,max:1000});draw();
    const parity=depths.map((d,i)=>colorAt(...records.slice(i*4,i*4+3),C.cloudColor(d,{min:0,max:1000}),'signed/range '+d));
    check(bindings.some(a=>a[1]===3&&a[2]===gl.SHORT&&a[3]===false&&a[4]===8&&a[5]===0),'position must bind signed SHORT stride 8 offset 0');
    check(bindings.some(a=>a[1]===1&&a[2]===gl.SHORT&&a[3]===false&&a[4]===8&&a[5]===6),'depth must bind signed SHORT stride 8 offset 6');
    same([...source.records],before,'source unchanged');source.records.fill(99);same([...view.records.slice(0,records.length)],records,'renderer owns copy');
    const uploadStart=uploads.length,geometryStart=view.uploads,pose=view.nav.pose(),reference=view.referenceDistance,uniformStart=view.uniformUpdates;
    view.setReverseDepthColors(true);draw();
    depths.forEach((d,i)=>colorAt(...records.slice(i*4,i*4+3),C.cloudColor(d,{min:0,max:1000},undefined,0,true),'reversed red-near/blue-far '+d));
    view.setColorRange(null);draw();colorAt(...records.slice(0,3),[.6,.67,.73],'neutral');
    for(const range of [{min:0,max:0},{min:NaN,max:1},{min:0,max:Infinity}]){view.setColorRange(range);draw();colorAt(...records.slice(4,7),[.6,.67,.73],'invalid neutral');}
    view.setReverseDepthColors(false);view.setColorRange({min:-32768,max:32767});draw();
    depths.forEach((d,i)=>colorAt(...records.slice(i*4,i*4+3),C.cloudColor(d,{min:-32768,max:32767}),'signed range '+d));
    style('circle',6,'perspective');draw();same(view.nav.pose(),pose,'uniform changes preserve pose');check(view.referenceDistance===reference,'stable reference');
    check(uploads.length===uploadStart&&view.uploads===geometryStart,'uniform/style changes must not upload');check(view.uniformUpdates>uniformStart,'uniform counter changes');
    const unchanged=view.uniformUpdates;view.setPointStyle({...view.pointStyle});view.setColorRange({...view.range});check(view.uniformUpdates===unchanged,'no-op setters do not count');
    // Coverage, CSS size and effective DPR, without relying on implementation-dependent edge pixels.
    view.update(snapshot([0,0,0,500]),{min:0,max:1000});style('square',10);draw();
    const center=at(0,0);colorAt(0,0,0,[0,1,0],'square center');same(pixel(center[0]+4,center[1]+4),[0,255,0],'square interior corner');
    style('circle',10);draw();same(pixel(center[0]+4,center[1]+4),clear,'circle discarded corner');colorAt(0,0,0,[0,1,0],'circle center');
    function width(){let n=0;for(let x=center[0]-50;x<=center[0]+50;x++)if(JSON.stringify(pixel(x,center[1]))===JSON.stringify([0,255,0]))n++;return n;}
    style('square',2);draw();check(Math.abs(width()-2)<=1,'small diameter');style('square',10);draw();check(Math.abs(width()-10)<=1,'large diameter');
    camera.position.z=200000;camera.updateMatrixWorld();draw();check(Math.abs(width()-10)<=1,'screen size invariant under camera distance');camera.position.z=100000;camera.updateMatrixWorld();
    // Resize uses actual capped renderer DPR, not raw devicePixelRatio.
    const dprs=[];
    for(const [raw,w,h] of [[1,256,256],[2,256,256],[2,5000,256]]){
        Object.defineProperty(window,'devicePixelRatio',{configurable:true,value:raw});host.style.width=w+'px';host.style.height=h+'px';view.resize();draw();
        const effective=view.counters().effectivePixelRatio;dprs.push({raw,effective,width:width()});
        check(Math.abs(width()-10*effective)<=1,'effective DPR size');check(view.canvas.width<=4096&&view.canvas.height<=4096,'drawing buffer cap');
        if(w===5000){style('square',1);draw();let covered=0;
            for(let x=center[0]-2;x<=center[0]+2;x++)for(let y=center[1]-2;y<=center[1]+2;y++)if(JSON.stringify(pixel(x,y))==='[0,255,0]')covered++;
            check(view.counters().hardwareSizeLimited&&covered===1,'exposed physical minimum limiting');style('square',10);}
    }
    Object.defineProperty(window,'devicePixelRatio',{configurable:true,value:1});host.style.width='256px';host.style.height='256px';view.resize();
    // Orthographic camera keeps sprite centers fixed while declared view depth changes.
    style('square',10,'perspective');view.referenceDistance=100000;view.material.uniforms.referenceDistance.value=100000;
    const perspective=[];
    for(const z of [50000,100000,400000]){camera.position.z=z;camera.far=1000000;camera.updateProjectionMatrix();camera.updateMatrixWorld();draw();const size=width();perspective.push(size);check(Math.abs(size-[20,10,5][perspective.length-1])<=1,'bounded perspective size');}
    camera.position.z=100000;camera.updateMatrixWorld();style('square',10);
    // Sensor-depth scaling follows returned D, even with a reversed viewing axis.
    // Fit captures a stable positive-D midpoint; ordinary frames/range/orbit do not.
    view.update(snapshot([0,0,0,500,1000,0,0,1500]),{min:0,max:2000});view.fit();
    check(view.referenceDepth===1000,'source D reference midpoint');style('square',10,'depth');
    const sensorSizes=[];
    for(const z of [50000,200000])for(const d of [-32768,0,500,1000,1500]){
        camera.position.z=z;camera.updateMatrixWorld();
        view.update(snapshot([0,0,0,d]),d>0?{min:0,max:2*d}:d<0?{min:2*d,max:0}:{min:-1,max:1});draw();
        const actual=width(),expected=C.pointSizePixels(view.pointStyle,1,view.pointSizeRange,view.referenceDistance,z,d,1000);
        check(Math.abs(actual-expected)<=1,'sensor near larger/far smaller: '+JSON.stringify({z,d,actual,expected}));
        check(view.referenceDepth===1000,'new frame must not pump sensor reference');sensorSizes.push({z,d,actual,expected});
    }
    camera.position.z=100000;camera.updateMatrixWorld();view.update(snapshot([0,0,0,500]),{min:0,max:1000});style('square',10);
    // Artificially lower the clamp uniform to prove the shader honors it even on hardware with a large range.
    view.material.uniforms.pointSizeRange.value.set(1,6);draw();check(Math.abs(width()-6)<=1,'shader upper hardware clamp');
    view.material.uniforms.pointSizeRange.value.set(12,20);draw();check(Math.abs(width()-12)<=1,'shader lower hardware clamp');
    view.material.uniforms.pointSizeRange.value.set(...view.pointSizeRange);
    // Near/behind-camera depths: actual shader must remain finite and normal clipping authoritative.
    const perspectiveCamera=new T.PerspectiveCamera(45,1,1,1000000);perspectiveCamera.position.set(0,0,0);perspectiveCamera.lookAt(0,0,-1);perspectiveCamera.updateMatrixWorld();
    const ortho=camera.clone();camera.copy(perspectiveCamera);style('square',10,'perspective');
    for(const z of [0,1,-1,-2]){view.update(snapshot([0,0,z,500]),{min:0,max:1000});draw();check(gl.getError()===gl.NO_ERROR,'finite degenerate point sizing');if(z>=0)same(pixel(...center),clear,'behind/zero clipped');if(z===-2)same(pixel(...center),[0,255,0],'near visible');}
    camera.copy(ortho);style('square',10);
    // Signed Z extrema: positive signed Z is nearer. Both input orders must occlude identically.
    for(const shape of ['circle','square'])for(const reverse of [false,true]){
        const pair=[[0,0,32767,750],[0,0,-32768,250]];if(reverse)pair.reverse();view.update(snapshot(pair.flat()),{min:0,max:1000});style(shape,10);draw();colorAt(0,0,0,[1,0,0],'opaque signed Z '+shape+' '+reverse);
    }
    // Maximum -> small -> empty -> nonempty uses one buffer and populated subData prefixes.
    const geometry=view.geometry,material=view.material,buffer=view.attributeBuffer,max=new Int16Array(C.CLOUD_MAX*4);max.fill(0);max[3]=500;
    view.update({...snapshot([]),records:max,recordCount:C.CLOUD_MAX},null);draw();const maxCounter=view.counters();
    view.update(snapshot([0,0,0,500]),{min:0,max:1000});draw();const smallUpload=uploads.at(-1);check(smallUpload.method==='bufferSubData'&&smallUpload.bytes===8,'small prefix upload 8 bytes: '+JSON.stringify(smallUpload));
    const beforeEmpty=uploads.length,emptyUploads=view.uploads;view.update(snapshot([]),null);draw();same(pixel(...center),clear,'empty clears stale points');check(uploads.length===beforeEmpty&&view.uploads===emptyUploads,'empty uploads no tail');
    view.update(snapshot([0,0,0,500]),{min:0,max:1000});draw();colorAt(0,0,0,[0,1,0],'nonempty after empty');
    const coalescedUploads=uploads.length;view.update({...snapshot([]),records:max,recordCount:C.CLOUD_MAX},null);view.update(snapshot([]),null);draw();
    check(uploads.length===coalescedUploads,'coalesced nonempty -> empty must suppress pending upload');
    view.update(snapshot([0,0,0,500]),{min:0,max:1000});draw();check(uploads.at(-1).bytes===8,'coalesced recovery uploads only populated prefix');
    check(view.geometry===geometry&&view.material===material&&view.attributeBuffer===buffer,'reusable resources');check(Object.keys(geometry.attributes).sort().join(',')==='cleanupFlag,depth,position','no RGB attribute');
    check(maxCounter.bufferBytes===2457600,'8 bytes/record capacity');check(view.renderer.info.programs.length===2,'no style program variants (points + axes)');
    const stableReference=view.referenceDistance;view.nav.zoom(.5);view.update(snapshot([0,0,0,500]),null);check(view.referenceDistance===stableReference,'new frame must not recapture size reference');view.fit();check(view.referenceDistance===view.nav.radius,'fit captures reference');view.nav.zoom(.5);view.reset();check(view.referenceDistance===view.nav.radius,'reset captures reference');
    const selected={shape:'square',diameterCssPx:7,sizing:'perspective'};view.setPointStyle(selected);selected.diameterCssPx=1;check(view.pointStyle.diameterCssPx===7,'style copied');
    const counters=view.counters(),savedStyle={...counters.pointStyle},programCount=view.renderer.info.programs.length;
    const report={revision:T.REVISION,webgl:gl.getParameter(gl.VERSION),renderer:gl.getParameter(gl.RENDERER),pointSizeRange:counters.pointSizeRange,
        parity,depths,dprs,perspective,sensorSizes,initialAllocation:uploads[0],smallUpload,counters,programCount,bindings:bindings.filter(a=>a[2]===gl.SHORT),observedUploadCalls:uploads.length};
    // Inspect real PNG footer drawing calls AND losslessly decoded exported pixels, including overlays off.
    view.renderer.setRenderTarget(null);view.render();
    const pngPose=view.nav.pose(),pngUploads=view.uploads,pngRecords=[...view.records.slice(0,4)],pngSnapshot=snapshot([0,0,0,500]);
    const fillText=CanvasRenderingContext2D.prototype.fillText,calls=[];report.pngProvenance=[];
    CanvasRenderingContext2D.prototype.fillText=function(...args){calls.push({canvas:this.canvas,args});return fillText.apply(this,args);};
    try{
        for(const entry of [
            {shape:'circle',diameterCssPx:4.5,sizing:'screen',overlays:false,source:'live',stale:true,range:{min:0,max:1000}},
            {shape:'square',diameterCssPx:7,sizing:'perspective',overlays:true,source:'demo',stale:false,range:{min:0,max:1000}},
            {shape:'square',diameterCssPx:1,sizing:'screen',overlays:false,source:'live',stale:false,range:null,capped:true},
            {shape:'circle',diameterCssPx:10,sizing:'perspective',overlays:true,source:'demo',stale:false,range:null}
        ]){
            Object.defineProperty(window,'devicePixelRatio',{configurable:true,value:entry.capped?2:1});
            host.style.width=(entry.capped?5000:256)+'px';view.resize();style(entry.shape,entry.diameterCssPx,entry.sizing);view.setColorRange(entry.range);calls.length=0;
            const blob=await view.png(pngSnapshot,entry),bytes=new Uint8Array(await blob.arrayBuffer());
            same([...bytes.slice(0,8)],[137,80,78,71,13,10,26,10],'actual PNG signature');check(blob.type==='image/png','PNG MIME');
            check(calls.length>=6,'wrapped provenance includes clipping regardless of overlay setting');
            const texts=calls.map(c=>c.args[0]),size=view.counters();
            const joined=texts.join(' ');
            check(joined.includes(entry.source==='demo'?'DEMO · SYNTHETIC DATA':'CHITON · LIVE API')&&joined.includes('cloud frame 7')&&joined.includes(entry.stale?'STALE':'snapshot'),'frame/source/stale preserved: '+joined);
            check(joined.includes('mm / calibration_aoi · API coordinates · 1 records'),'units/count preserved');
            check(joined.includes(`Depth color: ${entry.range?'0…1000 mm':'unavailable (neutral)'}`)&&joined.includes('Cloud and numeric measurement may be different frames'),'range/independent-frame provenance preserved');
            check(joined.includes('Clipping off')&&joined.includes('1 / 1 records match clipping'),'clipping/count provenance always present');
            const expected=`Style: ${entry.shape} ${entry.diameterCssPx} CSS px · ${entry.sizing} · DPR ${Number(size.effectivePixelRatio.toFixed(3))} · HW ${size.pointSizeRange[0]}…${size.pointSizeRange[1]} px${entry.capped?' · limited':''}`;
            check(joined.includes(expected),'current style/effective DPR/hardware provenance: '+joined);
            check(!view.axes.visible,'PNG restores overlay visibility');
            const exportCanvas=calls[0].canvas;
            for(const call of calls){const [text,x,y]=call.args;check(x>=0&&y>0&&y<exportCanvas.height&&x+exportCanvas.getContext('2d').measureText(text).width<=exportCanvas.width,'wrapped provenance fits export bounds');}
            const image=await createImageBitmap(blob),decoded=document.createElement('canvas');decoded.width=image.width;decoded.height=image.height;
            check(image.width===view.canvas.width&&image.height>view.canvas.height,'PNG preserves full viewport and appends provenance');
            const ctx=decoded.getContext('2d');ctx.drawImage(image,0,0);image.close();
            const top=view.canvas.height,height=decoded.height-top;
            const actual=ctx.getImageData(0,top,decoded.width,height).data,original=exportCanvas.getContext('2d').getImageData(0,top,decoded.width,height).data;
            check(actual.every((v,i)=>v===original[i]),'encoded PNG contains exact drawn style footer');
            let ink=0;for(let i=0;i<actual.length;i+=4)if(actual[i]>100&&actual[i+1]>100&&actual[i+2]>100)ink++;
            check(ink>20,'style footer text visible in actual PNG pixels');
            let binary='';for(const byte of bytes)binary+=String.fromCharCode(byte);
            report.pngProvenance.push({overlays:entry.overlays,shape:entry.shape,diameterCssPx:entry.diameterCssPx,sizing:entry.sizing,effectivePixelRatio:size.effectivePixelRatio,hardwareSizeLimited:size.hardwareSizeLimited,texts,dimensions:[decoded.width,decoded.height],styleInkPixels:ink,pngBase64:btoa(binary)});
        }
    }finally{CanvasRenderingContext2D.prototype.fillText=fillText;}
    check(view.uploads===pngUploads,'PNG/style/range changes do not request geometry uploads');same(view.nav.pose(),pngPose,'PNG preserves navigation');same([...view.records.slice(0,4)],pngRecords,'PNG preserves packed records');
    target.dispose();view.dispose();view.dispose();check(!host.children.length&&view.raf===null,'dispose canvas/RAF');
    const recreated=new C.CloudView(host);recreated.setPointStyle(savedStyle);same(recreated.counters().pointStyle,savedStyle,'caller restores style on recreation');recreated.dispose();
    // A real shader compile failure must throw to the constructor unavailable/retry seam and clean up.
    const create=C.createCloudMaterial;C.createCloudMaterial=(...args)=>{const m=create(...args);m.vertexShader='intentional invalid GLSL';return m;};
    let failed=false;try{new C.CloudView(host);}catch(e){failed=/shader compilation\/link failed/.test(e.message);}finally{C.createCloudMaterial=create;}
    check(failed&&!host.children.length,'shader compile failure/cleanup');report.compileFailureCleanedUp=failed;
    let message=null;const broken=new C.CloudView(host,{onUnavailable:m=>{message=m;}});broken.checkPrograms=()=>{throw new Error('injected render failure');};broken.render();
    check(message?.includes('WebGL rendering failed')&&!broken.active&&broken.raf===null,'render failure reports unavailable/stops paints');report.renderFailureMessage=message;broken.dispose();
    check(gl.getError()===gl.NO_ERROR||gl.isContextLost(),'no GL error');
    return report;
}
(async()=>{
    const assets=path.resolve('release/files'),sockets=new Set();
    const server=http.createServer((req,res)=>{
        if(new URL(req.url,'http://127.0.0.1').pathname==='/smoke.html'){res.setHeader('Content-Type','text/html; charset=utf-8');res.end('<!doctype html><script>if(location.search.includes("webgl1")){const get=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(type,...args){return type==="webgl2"?null:get.call(this,type,...args);};}</script><div id="host" style="width:256px;height:256px"></div>'+['three.min.js','console-model.js','console-cloud.js','console-cloud-material.js','console-cloud-view.js'].map(n=>'<script src="/'+n+'"></script>').join('')+'<script>window.CONSOLE_READY=true;</script>');return;}
        if(!/^\/[a-z-]+\.(min\.)?js$/.test(req.url)){res.writeHead(404);res.end();return;}
        const file=path.join(assets,req.url.slice(1));if(!fs.existsSync(file)){res.writeHead(404);res.end();return;}res.setHeader('Content-Type','application/javascript; charset=utf-8');res.end(fs.readFileSync(file));
    });
    server.on('connection',s=>{sockets.add(s);s.on('close',()=>sockets.delete(s));});
    await new Promise(r=>server.listen(0,'127.0.0.1',r));let b;
    try{
        const origin='http://127.0.0.1:'+server.address().port;b=await browserFor(origin);await b.viewport(800,600);
        for(const mode of ['webgl2','webgl1']){
            await b.navigate(origin+'/smoke.html?'+mode);
            const report=await b.evaluate('('+framebufferChecks.toString()+')()');assert.deepEqual(b.errors,[]);assert(!b.requests.some(u=>!u.startsWith(origin+'/')));
            assert.match(report.webgl,mode==='webgl1'?/WebGL 1.0/:/WebGL 2.0/);
            report.browser=await b.command('Browser.getVersion',{},null);report.passed=true;report.externalRequests=[];
            const out=path.resolve(process.env.CHITON_RENDERER_EVIDENCE||'_build/pointcloud-renderer-style/validation');fs.mkdirSync(out,{recursive:true});
            for(const [i,png] of report.pngProvenance.entries()){
                png.file='renderer-png-'+mode+'-'+i+'.png';fs.writeFileSync(path.join(out,png.file),Buffer.from(png.pngBase64,'base64'));delete png.pngBase64;
            }
            fs.writeFileSync(path.join(out,'renderer-gpu-'+mode+'.json'),JSON.stringify(report,null,2));
            console.log('console_cloud_material_gpu '+mode+': actual r128 signed SHORT XYZD extrema/negative D, color parity <=2/255, neutral/black endpoints, circle/square, opacity/draw order, CSS/effective DPR/clamps, perspective bounds, uniform-only GL calls, max/small/empty/coalesced reuse, actual PNG style provenance with/without overlays, disposal and shader failure flow passed');
            console.log(JSON.stringify({bufferBytes:report.counters.bufferBytes,initialAllocation:report.initialAllocation,smallUpload:report.smallUpload,dprs:report.dprs,perspective:report.perspective,pointSizeRange:report.pointSizeRange}));
        }
    }finally{if(b)await b.close();for(const s of sockets)s.destroy();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
