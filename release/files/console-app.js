/* Viewer-local orchestration. Only sources perform device network operations. */
(function(root){
    'use strict';
    const C=root.ChitonConsole,{$,text}=C,query=new URLSearchParams(location.search);
    const sourceName=query.get('source')||'demo';
    if(!['demo','live'].includes(sourceName)){
        text('notice','Unknown source. Choose ?source=demo or ?source=live; no data source was started.');$('notice').classList.add('fatal');return;
    }
    let source;
    try{source=sourceName==='demo'?new C.DemoSource({seed:Number(query.get('seed')||7),scenario:query.get('scenario')||'stable'}):new C.LiveSource();}
    catch(error){text('notice',error.message);$('notice').classList.add('fatal');return;}
    const view=new C.View();
    const s={sourceName,selection:{kind:'point',x:460,y:320},device:{mode:null,frameType:null,capture:null},latest:null,
        context:null,history:new C.Ring(300),distinctTimes:[],phase:'connecting',latency:null,skipped:0,reference:null,
        filter:'off',kalman:new C.Kalman(),estimate:null,segment:0,hz:20,pointHz:20,roiHz:5,windowSeconds:10,
        precision:1,held:null,paused:false,mutation:false,unitConfig:null};
    if(query.get('scenario')==='roi')s.selection={kind:'roi',x:420,y:280,x2:500,y2:360};
    let clientId=crypto.randomUUID(),frameHandle=null,frameController=null,uiTimer=null,running=false,generation=0,drag=null;
    const objectURLs=new Set(),operations=new Set();
    async function boundedOperation(operation,ms=2000){
        const controller=new AbortController();operations.add(controller);
        try{return await C.bounded(operation,controller.signal,ms);}finally{operations.delete(controller);}
    }
    const narrow=matchMedia('(max-width:899px)');
    function readingOrder(){const history=$('history').closest('section');if(narrow.matches)$('measurement').after(history);else document.querySelector('.workspace').append(history);}
    narrow.addEventListener('change',readingOrder);readingOrder();
    function notice(message,error=false){text('notice',message);$('notice').classList.toggle('error',error);}
    function resetLocal(reason,clearLatest=true){
        s.history.clear();s.distinctTimes=[];s.kalman.reset(reason);s.estimate=null;s.reference=null;s.context=null;s.segment++;s.skipped=0;s.held=null;
        $('history-held').hidden=true;text('hold-history','Hold history');if(clearLatest)s.latest=null;view.schedule(s);
    }
    function syncSelection(){
        const roi=s.selection.kind==='roi';$('roi-coordinates').hidden=!roi;
        $('point-mode').setAttribute('aria-pressed',String(!roi));$('roi-mode').setAttribute('aria-pressed',String(roi));
        for(const [key,id] of [['x','coord-x'],['y','coord-y'],['x2','coord-x2'],['y2','coord-y2']])if(s.selection[key]!=null)$(id).value=s.selection[key];
        const width=s.device.mode>=5?1500:640,height=s.device.mode>=5?165:480;
        $('coord-x').max=width-1;$('coord-y').max=height-1;$('coord-x2').max=width;$('coord-y2').max=height;
        $('roi-mode').disabled=s.device.mode>=5||s.device.mode===4;
        text('selection-hint',roi?'Drag a region · exclusive far edges':'Click to select · arrow keys to adjust');view.drawOverlay(s);
    }
    function setSelection(selection){
        const width=s.device.mode>=5?1500:640,height=s.device.mode>=5?165:480;
        s.selection=C.selection(selection,width,height);generation++;measurement.stop();resetLocal('Selection changed');
        s.hz=s.selection.kind==='roi'?s.roiHz:s.pointHz;measurement.hz=s.hz;syncSelection();if(running)measurement.start();
    }
    function accept(m,info={latency:0},received=performance.now()){
        s.latency=info.latency;
        if((s.selection.kind==='roi')!==(m.statisticsKind==='spatial'))throw new Error('Wrong statistics scope');
        const context=C.context(m,s.selection);
        const changed=context!==s.context||m.historyReset;
        if(changed){resetLocal(m.historyReset?'API history reset':'Processing context changed');s.context=context;}
        if(s.latest && m.frameId<s.latest.frameId){
            if(s.reconnecting || m.historyReset){resetLocal('Confirmed frame restart');s.context=context;}
            else return;
        }
        s.phase='live';s.reconnecting=false;
        if(s.latest && m.frameId===s.latest.frameId){s.latest={...s.latest,frameAgeMs:m.frameAgeMs,received};view.schedule(s);return;}
        const old=s.latest;
        if(old)s.skipped+=Math.max(0,m.frameId-old.frameId-1);
        const dt=old&&m.capturedAtMs!=null&&old.capturedAtMs!=null?m.capturedAtMs-old.capturedAtMs:null;
        if(old && (received-old.received>2000 || (dt!=null&&(dt<=0||dt>2000)))){s.kalman.reset('Outage / timestamp discontinuity');s.segment++;}
        if(s.filter!=='off' && (m.frameType!=='depth'||s.selection.kind!=='point'||s.unitConfig!==m.units)){
            s.filter='off';$('filter').value='off';s.kalman.reset('Incompatible filter context');s.estimate=null;
        }
        s.estimate=s.filter!=='off'?s.kalman.update(m.value,m.capturedAtMs,m.frameId):null;
        s.latest={...m,received,distinctReceived:received};s.distinctTimes.push(received);s.distinctTimes=s.distinctTimes.filter(t=>t>=received-2000).slice(-60);
        s.history.push({...m,received,estimate:s.estimate,segment:s.segment,q:s.filter==='off'?null:s.kalman.q,R:s.filter==='off'?null:s.kalman.R,selection:{...s.selection}});
        view.schedule(s);
    }
    const measurement=new C.Scheduler({hz:20,read:async signal=>{
        const revision=generation,args={clientId,selection:{...s.selection},signal};
        const m=await source.readMeasurement(args);if(revision!==generation)throw new Error('Obsolete measurement');return m;
    },onValue:accept,onError:error=>{s.phase='error';s.reconnecting=true;notice('Measurement delayed: '+error.message,true);view.schedule(s);}});
    function startFrames(){
        frameController?.abort();frameHandle?.stop();frameController=new AbortController();
        if(!running || s.device.mode===4){text('image-state',s.device.mode===4?'3D · use legacy viewer':'Image paused');return;}
        frameHandle=source.startFrames({signal:frameController.signal,fps:()=>Number($('image-rate').value),
            onFrame:(image,m)=>{if(!running)return false;return view.drawFrame(image,m,s);},
            onStatus:p=>{if(p.capture_enabled===false)text('image-state','Image · capture disabled');},
            onError:error=>{text('image-state','Image delayed · '+error.message);}});
    }
    function applyStatus(device){
        const changed=s.device.mode!==device.mode||s.device.frameType!==device.frameType;
        const oldMode=s.device.mode;s.device=device;
        $('view-mode').value=device.mode;$('frame-type').value=device.frameType||'';
        $('cloud-message').hidden=device.mode!==4;
        if(device.range){if(document.activeElement!==$('color-min'))$('color-min').value=device.range.min;if(document.activeElement!==$('color-max'))$('color-max').value=device.range.max;}
        if(changed){
            generation++;measurement.stop();resetLocal('Acknowledged acquisition / mapping change');
            if((oldMode>=5)!==(device.mode>=5))s.selection={kind:'point',x:device.mode>=5?750:460,y:device.mode>=5?82:320};
            s.hz=s.selection.kind==='roi'?s.roiHz:s.pointHz;measurement.hz=s.hz;syncSelection();view.clearImage();startFrames();if(running)measurement.start();
        }
        if(!s.mutation)notice(sourceName==='demo'?'DEMO · Synthetic scene. Viewer controls are local; device actions are simulated.':'LIVE API · Device-wide changes require an explicit action. No hardware qualification claimed.');
        view.schedule(s);
    }
    const status=new C.Scheduler({hz:1,read:signal=>source.readStatus({signal}),onValue:applyStatus,onError:error=>{notice('Status unavailable: '+error.message,true);}});
    const temperature=new C.Scheduler({hz:.25,read:signal=>source.readTemperature({signal}),onValue:p=>text('temperatures',`CPU ${C.format(p.cpu,1)} °C · Vixel ${C.format(p.vixel,1)} °C`),onError:()=>text('temperatures','CPU / Vixel unavailable (read failed)')});
    const metrics=sourceName==='live'?new root.ChitonMetricsReader(p=>text('diagnostics',root.chitonFormatDiagnostics(p))):null;
    let metricsTimer=null;
    function stop(){
        running=false;generation++;measurement.stop();status.stop();temperature.stop();frameController?.abort();frameHandle?.stop();source.stop();
        clearInterval(uiTimer);clearInterval(metricsTimer);uiTimer=metricsTimer=null;metrics?.stop();view.stop();
        for(const operation of operations)operation.abort(new Error('Viewer stopped'));
    }
    function start(){
        if(running||s.paused||document.hidden)return;running=true;s.phase='connecting';s.frozenNow=null;
        status.start();measurement.start();startFrames();if(sourceName==='live')temperature.start();
        uiTimer=setInterval(()=>view.schedule(s),100);if(metrics)metricsTimer=setInterval(()=>metrics.poll(false),1000);
    }
    function pause(){s.paused=!s.paused;if(s.paused){stop();s.phase='paused';view.render(s,performance.now());}else start();text('pause-viewer',s.paused?'Resume viewer':'Pause viewer');}
    function download(blob,filename){
        const url=URL.createObjectURL(blob);objectURLs.add(url);const anchor=document.createElement('a');anchor.href=url;anchor.download=filename;anchor.click();
        setTimeout(()=>{URL.revokeObjectURL(url);objectURLs.delete(url);},1000);
    }
    async function action(operation){
        try{await operation();}catch(error){notice(error.message,true);text('settings-result',error.message);}
    }
    async function mutate(name,value){
        if(s.mutation)throw new Error('A device setting is already pending');
        if(sourceName==='live'&&!confirm('This changes the device for every viewer. Apply '+name+'?'))return;
        s.mutation=true;stop();resetLocal('Setting pending');text('settings-result','Applying once…');notice('Setting pending · '+name);
        const controls=[$('frame-type'),$('view-mode'),...$('settings').querySelectorAll('form button')];controls.forEach(b=>b.disabled=true);
        try{await boundedOperation(signal=>source.applySetting({name,value,signal}));text('settings-result',sourceName==='demo'?'Simulated setting acknowledged.':'Setting acknowledged. Read-back pending.');}
        catch(error){text('settings-result','Outcome unknown: '+error.message+'. No retry. Re-reading available state.');notice('Setting outcome unknown; no retry.',true);}
        finally{s.mutation=false;controls.forEach(b=>b.disabled=false);if(sourceName==='demo')$('quality-settings').querySelector('button').disabled=true;$('view-mode').value=s.device.mode??'';$('frame-type').value=s.device.frameType??'';start();}
    }
    function filterChanged(preset){
        if(preset==='off'){s.filter='off';s.estimate=null;s.kalman.reset('Filter off');s.segment++;view.schedule(s);return;}
        const m=s.latest;if(!m||m.frameType!=='depth'||s.selection.kind!=='point')throw new Error('Kalman requires an acknowledged depth point');
        if(sourceName==='live'){
            if(m.units!=='mm'&&s.unitConfig!==m.units)throw new Error('Set unit-specific R and q in filter details before enabling for sensor units');
            if(!confirm('These are local display estimates, not improved sensor accuracy. Acknowledge R/q noise tuning in '+m.units+' before enabling?')){$('filter').value=s.filter;return;}
        }
        if(m.units==='mm'){s.kalman.configure(C.presets[preset]);$('filter-r').value=s.kalman.R;$('filter-q').value=s.kalman.q;}
        s.unitConfig=m.units;s.filter=preset;s.segment++;s.estimate=null;view.schedule(s);
    }
    $('pause-viewer').onclick=pause;
    $('coordinates').onsubmit=event=>{event.preventDefault();action(()=>setSelection({kind:s.selection.kind,x:Number($('coord-x').value),y:Number($('coord-y').value),...s.selection.kind==='roi'?{x2:Number($('coord-x2').value),y2:Number($('coord-y2').value)}:{}}));};
    $('point-mode').onclick=()=>setSelection({kind:'point',x:s.selection.x,y:s.selection.y});
    $('roi-mode').onclick=()=>setSelection({kind:'roi',x:Math.min(600,s.selection.x),y:Math.min(440,s.selection.y),x2:Math.min(640,s.selection.x+40),y2:Math.min(480,s.selection.y+40)});
    function pointer(event,edge=false){return C.mapPoint(event.clientX,event.clientY,view.sensor.getBoundingClientRect(),view.sensor.width,view.sensor.height,edge);}
    view.sensor.onpointerdown=event=>{
        if(event.button!==0||s.device.mode===4)return;const p=pointer(event);if(!p)return;
        if(s.selection.kind==='point'){setSelection({kind:'point',...p});return;}
        drag=p;view.sensor.setPointerCapture(event.pointerId);measurement.stop();
    };
    view.sensor.onpointermove=event=>{
        if(!drag)return;const p=pointer(event,true);if(!p)return;
        s.dragSelection={kind:'roi',x:Math.min(drag.x,p.x),y:Math.min(drag.y,p.y),x2:Math.max(drag.x+1,p.x),y2:Math.max(drag.y+1,p.y)};view.drawOverlay(s);
    };
    function endDrag(event,cancel=false){
        if(!drag)return;const selected=s.dragSelection;drag=null;s.dragSelection=null;
        if(view.sensor.hasPointerCapture(event.pointerId))view.sensor.releasePointerCapture(event.pointerId);
        if(selected&&!cancel)setSelection(selected);else{view.drawOverlay(s);if(running)measurement.start();}
    }
    view.sensor.onpointerup=e=>endDrag(e);view.sensor.onpointercancel=e=>endDrag(e,true);view.sensor.onlostpointercapture=e=>endDrag(e,true);
    view.sensor.onkeydown=event=>{
        const moves={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]};if(!moves[event.key]||s.selection.kind!=='point')return;event.preventDefault();
        const [dx,dy]=moves[event.key],step=event.shiftKey?10:1;setSelection({kind:'point',x:Math.max(0,Math.min(view.sensor.width-1,s.selection.x+dx*step)),y:Math.max(0,Math.min(view.sensor.height-1,s.selection.y+dy*step))});
    };
    $('show-overlay').onchange=()=>view.drawOverlay(s);
    $('fit').onclick=()=>{view.sensor.scrollIntoView({block:'nearest'});view.drawOverlay(s);};
    $('fullscreen').onclick=()=>action(async()=>{if(document.fullscreenElement)await document.exitFullscreen();else await $('viewport').requestFullscreen();});
    $('reference').onclick=()=>{if(s.latest)s.reference=s.latest.value;view.schedule(s);};$('clear-reference').onclick=()=>{s.reference=null;view.schedule(s);};
    $('filter').onchange=()=>action(()=>{try{filterChanged($('filter').value);}catch(e){$('filter').value=s.filter;throw e;}});
    $('filter-parameters').onsubmit=event=>{event.preventDefault();action(()=>{
        if(!s.latest)throw new Error('Read a sample before configuring units');
        s.kalman.configure({q:Number($('filter-q').value),R:Number($('filter-r').value)});s.unitConfig=s.latest.units;s.segment++;s.estimate=null;
        text('settings-result','Local variances configured for '+s.unitConfig);view.schedule(s);
    });};
    $('hold-history').onclick=()=>{s.held=s.held?null:s.history.items.map(v=>({...v}));$('history-held').hidden=!s.held;text('hold-history',s.held?'Resume history':'Hold history');view.schedule(s);};
    $('history-window').onchange=()=>{s.windowSeconds=Number($('history-window').value);view.schedule(s);};
    $('csv').onclick=()=>download(new Blob([C.csv(s.history.items.map(v=>({...v,selection:{...v.selection}})))],{type:'text/csv;charset=utf-8'}),'chiton-'+sourceName+'-measurements.csv');
    $('png').onclick=()=>action(async()=>download(await view.png(s,$('png-overlays').checked),'chiton-'+sourceName+'.png'));
    for(const format of ['raw','obj'])$(format).onclick=()=>action(async()=>{const result=await boundedOperation(signal=>source.exportFrame({format,signal}),5000);download(result.blob,result.filename);});
    $('view-mode').onchange=()=>action(()=>mutate('mode',Number($('view-mode').value)));
    $('frame-type').onchange=()=>action(()=>mutate('frameType',$('frame-type').value));
    $('device-settings').onsubmit=e=>{e.preventDefault();action(()=>mutate($('processing-setting').value,$('processing-value').value==='true'));};
    $('range-settings').onsubmit=e=>{e.preventDefault();action(()=>mutate('range',{min:Number($('color-min').value),max:Number($('color-max').value)}));};
    $('quality-settings').onsubmit=e=>{e.preventDefault();action(()=>mutate('quality',Number($('jpeg-quality').value)));};
    $('point-rate').onchange=()=>{s.pointHz=Number($('point-rate').value);if(s.selection.kind==='point'){s.hz=s.pointHz;if(running)measurement.restart(s.hz);else measurement.hz=s.hz;}};
    $('roi-rate').onchange=()=>{s.roiHz=Number($('roi-rate').value);if(s.selection.kind==='roi'){s.hz=s.roiHz;if(running)measurement.restart(s.hz);else measurement.hz=s.hz;}};
    $('image-rate').onchange=()=>{if(running)startFrames();};
    function preference(){
        document.body.dataset.size=$('text-size').value;s.precision=Number($('precision').value);view.schedule(s);
        try{localStorage.setItem('chiton-console-display',JSON.stringify({size:$('text-size').value,precision:s.precision}));}catch(_){}
    }
    try{const p=JSON.parse(localStorage.getItem('chiton-console-display'));if(p&&['normal','large','presentation'].includes(p.size))$('text-size').value=p.size;if(p&&[0,1,2].includes(p.precision))$('precision').value=p.precision;}catch(_){}
    $('text-size').onchange=preference;$('precision').onchange=preference;preference();
    $('open-settings').onclick=()=>$('settings').showModal();$('close-settings').onclick=()=>$('settings').close();
    $('settings').addEventListener('close',()=>$('open-settings').focus());
    for(const scenario of C.scenarios){const option=document.createElement('option');option.value=scenario;option.textContent=scenario.replaceAll('-',' ');$('scenario').append(option);}
    $('scenario').value=source.scenario||'stable';
    function reloadScenario(){const url=new URL(location.href);url.searchParams.set('scenario',$('scenario').value);url.searchParams.set('source','demo');location.assign(url);}
    $('scenario').onchange=reloadScenario;$('demo-reset').onclick=reloadScenario;
    $('demo-controls').hidden=sourceName!=='demo';$('raw').disabled=$('obj').disabled=sourceName==='demo';
    text('source-badge',sourceName==='demo'?'DEMO · SYNTHETIC DATA':'LIVE · SAME-ORIGIN API');text('viewport-source',sourceName==='demo'?'DEMO · SYNTHETIC':'LIVE API');
    text('source-description',sourceName==='demo'?'Local synthetic numeric scene. No device APIs are contacted.':'Existing APIs on this page’s origin. Failures never substitute demo data.');
    if(sourceName==='demo'){
        text('device-heading','Simulated device settings');text('device-warning','Simulated only. Color range affects Color/Color panorama, not grayscale. No device API calls are made.');
        $('jpeg-quality').disabled=true;$('quality-settings').querySelector('button').disabled=true;
        const explanation=document.createElement('p');explanation.textContent='JPEG quality is unavailable in demo: this source renders a numeric canvas, not JPEGs.';$('quality-settings').append(explanation);
    }
    $('switch-source').href=sourceName==='demo'?'?source=live':'?source=demo';text('switch-source',sourceName==='demo'?'Switch explicitly to same-origin live APIs →':'Switch explicitly to synthetic demo →');
    document.addEventListener('visibilitychange',()=>{if(document.hidden){stop();s.phase='paused';}else start();});
    root.addEventListener('pagehide',()=>{stop();for(const url of objectURLs)URL.revokeObjectURL(url);objectURLs.clear();});
    root.addEventListener('pageshow',()=>start());root.addEventListener('resize',()=>{view.drawOverlay(s);view.schedule(s);});
    // Local automation: real source samples pre-rolled at declared observation times, not fabricated API startup history.
    async function prepareCapture({timeMs=6000,filter=false}={}){
        if(sourceName!=='demo')throw new Error('Freeze is available only in demo mode');
        stop();source.seek(0);clientId=crypto.randomUUID();resetLocal('Deterministic review pre-roll');
        applyStatus(await source.readStatus());
        if(filter){s.unitConfig='mm';s.filter='Balanced';s.kalman.configure(C.presets.Balanced);$('filter').value='Balanced';}
        const end=query.get('scenario')==='warming'?0:timeMs,reviewNow=performance.now();s.frozenNow=reviewNow;
        let imageSnapshot=null,imageTime=0;
        for(let t=Math.max(0,end-10000);t<=end;t+=1000/s.hz){
            source.seek(t);
            try{imageSnapshot=source.frameSnapshot();imageTime=t;}catch(_){/* Keep the last actually available image. */}
            try{const m=await source.readMeasurement({clientId,selection:s.selection});accept(m,{latency:0},reviewNow-(end-t));}
            catch(error){s.phase='error';s.reconnecting=true;notice('Simulated disconnect: '+error.message,true);}
        }
        source.seek(end);
        if(imageSnapshot)view.drawFrame({synthetic:true,source,metadata:imageSnapshot},{id:imageSnapshot.frame_id,mode:source.mode,age:imageSnapshot.frame_age_ms,reviewReceived:reviewNow-(end-imageTime)},s);
        view.render(s,performance.now());await document.fonts.ready;await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));
        root.CONSOLE_READY=true;return {source:sourceName,scenario:source.scenario,seed:source.seed,timeMs:end,frame:source.metadata().frame_id};
    }
    root.ChitonConsoleApp={state:s,source,view,measurement,status,start,stop,setSelection,prepareCapture,accept};
    syncSelection();start();root.CONSOLE_READY=true;
})(window);
