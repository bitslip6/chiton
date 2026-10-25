/* Viewer-local orchestration. Only sources perform device network operations. */
(function(root){
    'use strict';
    const C=root.ChitonConsole,{$,text}=C,query=new URLSearchParams(location.search);
    // Move existing nodes once, before source startup. Normal/fullscreen share one tree.
    const panel=$('display-controls'),opener=$('toggle-display-controls');
    const actions=document.querySelector('.display-actions');
    actions.append($('pause-viewer'),$('fullscreen'));
    $('display-navigation').append(document.querySelector('.toolbar'),$('reset-cloud'),$('cloud-navigation'),document.querySelector('.selection-bar'));
    $('display-cloud-controls').append($('point-appearance'),$('cloud-clipping'));
    $('display-refresh').append($('viewer-settings'),document.querySelector('.overlay-toggle'));
    $('display-export').append($('viewer-exports'),$('obj'),$('cloud-export-note'),$('clip-export-note'));
    $('display-help').append(document.querySelector('#cloud-navigation details'),$('open-settings'));
    $('display-status').append($('clip-status'),$('clip-context-notice'),$('cloud-framing-status'));
    $('point-appearance').append(document.querySelector('.cloud-edl-toggle'),$('cloud-edl-note'));
    document.querySelector('.viewport-top').prepend($('source-badge'));
    function setControls(open,{focus=false}={}){
        panel.hidden=!open;opener.setAttribute('aria-expanded',String(open));
        if(focus)(open?$('close-display-controls'):opener).focus({preventScroll:true});
    }
    opener.onclick=()=>setControls(panel.hidden,{focus:!panel.hidden});
    $('close-display-controls').onclick=()=>setControls(false,{focus:true});
    panel.addEventListener('keydown',event=>{
        if(event.key==='Escape'){event.preventDefault();event.stopPropagation();setControls(false,{focus:true});}
    });
    let settingsReturn=null;
    $('open-settings').onclick=()=>{
        settingsReturn={open:!panel.hidden,focus:document.activeElement};
        // The native modal must remain within the viewport-only fullscreen root.
        $('viewport').append($('settings'));setControls(false);$('settings').showModal();
    };
    $('close-settings').onclick=()=>$('settings').close();
    $('settings').addEventListener('close',()=>{
        const previous=settingsReturn;settingsReturn=null;setControls(!!previous?.open);
        const target=previous?.focus;
        (target?.isConnected&&target.getClientRects().length&&!target.disabled?target:opener).focus({preventScroll:true});
    });
    document.addEventListener('fullscreenchange',()=>text('fullscreen',document.fullscreenElement===$('viewport')?'Exit fullscreen':'Fullscreen'));
    const sourceName=query.get('source')||'live';
    if(!['demo','live'].includes(sourceName)){
        text('notice','Unknown source. Choose ?source=demo or ?source=live; no data source was started.');$('notice').classList.add('fatal');$('notice').hidden=false;return;
    }
    let source;
    try{source=sourceName==='demo'?new C.DemoSource({seed:Number(query.get('seed')||7),scenario:query.get('scenario')||'stable'}):new C.LiveSource();}
    catch(error){text('notice',error.message);$('notice').classList.add('fatal');$('notice').hidden=false;return;}
    const view=new C.View();
    const s={sourceName,selection:{kind:'point',x:460,y:320},device:{mode:null,frameType:null,capture:null},latest:null,
        context:null,history:new C.Ring(300),distinctTimes:[],phase:'connecting',latency:null,skipped:0,reference:null,
        filter:'off',kalman:new C.Kalman(),estimate:null,segment:0,hz:20,pointHz:20,roiHz:5,windowSeconds:10,
        precision:1,held:null,paused:false,mutation:false,unitConfig:null,cloud:new C.CloudTracker(),cloudHz:2,cloudUnavailable:null,objPending:false,pointStyle:C.validatePointStyle(C.DEFAULT_POINT_STYLE),cloudProjection:'perspective',eyeDomeLighting:false,
        clipSettings:C.defaultClipSettings(),clipDraft:C.defaultClipSettings(),clipContext:null,clipContextNotice:''};
    if(query.get('scenario')==='roi')s.selection={kind:'roi',x:420,y:280,x2:500,y2:360};
    let clientId=C.createClientId(),frameHandle=null,frameController=null,uiTimer=null,running=false,generation=0,drag=null,representationGeneration=0;
    const objectURLs=new Set(),operations=new Set();
    async function boundedOperation(operation,ms=2000){
        const controller=new AbortController();operations.add(controller);
        try{return await C.bounded(operation,controller.signal,ms);}finally{operations.delete(controller);}
    }
    const narrow=matchMedia('(max-width:899px)');
    function readingOrder(){const history=$('history').closest('section');if(narrow.matches)$('measurement').after(history);else document.querySelector('.workspace').append(history);}
    narrow.addEventListener('change',readingOrder);readingOrder();
    function notice(message,error=false){text('notice',message);$('notice').hidden=!message;$('notice').classList.toggle('error',error);}
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
        text('selection-hint',s.device.mode===4?'Numeric sensor pixel only':roi?'Drag a region · exclusive far edges':'Click to select · arrow keys to adjust');view.drawOverlay(s);
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
    function syncPointAppearance(){
        $('point-shape').value=s.pointStyle.shape;$('point-diameter').value=s.pointStyle.diameterCssPx;
        $('point-sizing').value=s.pointStyle.sizing;text('point-diameter-value',s.pointStyle.diameterCssPx.toFixed(1)+' CSS px');
        $('point-appearance').hidden=s.device.mode!==4;
        const available=s.device.mode===4&&s.device.frameType==='depth'&&!!view.cloud&&!s.cloudUnavailable&&s.device.capture!==false;
        $('point-appearance-controls').disabled=!available;view.syncCloudRenderingControls(s);syncClipping();
        text('point-appearance-note',available?'Local rendering and viewport PNG only. No device or measurement changes. Perspective scale is bounded to 0.5–2×; hardware may limit size.':'3D appearance unavailable. Settings are retained for this page.');
    }
    function setPointAppearance(style){
        s.pointStyle=C.validatePointStyle(style);view.cloud?.setPointStyle(s.pointStyle);syncPointAppearance();
        if(running)view.schedule(s);
    }
    function readClipDraft(){
        const value=id=>$(id).valueAsNumber;
        return {depth:{enabled:$('clip-depth-enabled').checked,min:value('clip-d-min'),max:value('clip-d-max')},
            box:{enabled:$('clip-box-enabled').checked,min:['x','y','z'].map(a=>value('clip-'+a+'-min')),max:['x','y','z'].map(a=>value('clip-'+a+'-max'))}};
    }
    function writeClipDraft(draft){
        s.clipDraft=C.validateClipSettings(draft);$('clip-depth-enabled').checked=draft.depth.enabled;$('clip-box-enabled').checked=draft.box.enabled;
        for(const end of ['min','max']){$('clip-d-'+end).value=draft.depth[end];for(let k=0;k<3;k++)$('clip-'+'xyz'[k]+'-'+end).value=draft.box[end][k];}
        text('clip-error','');$('clip-error').hidden=true;
    }
    function syncClipping(){
        const counters=view.cloud?.counters(),active=C.clippingActive(s.clipSettings),available=s.device.mode===4&&s.device.frameType==='depth'&&!!view.cloud&&!s.cloudUnavailable&&s.device.capture!==false;
        $('cloud-clipping').hidden=s.device.mode!==4;$('clip-controls').disabled=!available;$('clip-reset').disabled=!available;
        $('clip-bounds').disabled=!available||!s.cloud.latest?.recordCount;
        $('clip-status').hidden=s.device.mode!==4||(!active&&!$('clip-details').open&&JSON.stringify(s.clipDraft)===JSON.stringify(s.clipSettings));
        $('clip-active').hidden=!active;text('clip-applied',C.clipDescription(s.clipSettings).join(' · '));
        text('clip-count',counters?.clipStatsFrameId==null?'No cloud retained':`${counters.matchingCount} / ${counters.submittedCount} records match clipping${counters.matchingCount===0?' · No records match clipping; axes/overlays may remain':''}`);
        text('clip-draft-status',JSON.stringify(s.clipDraft)===JSON.stringify(s.clipSettings)?'Draft matches applied settings':'Unapplied changes — display and PNG use applied settings');
        text('clip-context-notice',s.clipContextNotice);$('clip-context-notice').hidden=s.device.mode!==4||!s.clipContextNotice;$('clip-export-note').hidden=!active;
        const m=s.cloud.latest,unit=m?.units==='mm'?'mm (calibration AOI only; not a validity guarantee)':m?.units||'Source units unavailable';
        text('clip-units',`${unit}${m?' / '+m.unitsScope:''}. API XYZ and returned signed D; no OBJ /6000 normalization.`);
    }
    function resetClipping(message=''){
        s.clipSettings=C.defaultClipSettings();writeClipDraft(s.clipSettings);s.clipContextNotice=message;view.cloud?.setClipSettings(s.clipSettings);syncClipping();
    }
    function acceptClipContext(snapshot,restart=false){
        const key=C.cloudClipContext(snapshot);
        if((s.clipContext!==null&&key!==s.clipContext)||restart)resetClipping(restart?'Clipping reset: confirmed source/process restart':'Clipping reset: coordinate context changed');
        s.clipContext=key;
    }
    function createCloudView(){
        const renderer=new C.CloudView($('cloud-host'),{onUnavailable:message=>unavailable(message,true)});
        renderer.setClipSettings(s.clipSettings);renderer.setPointStyle(s.pointStyle);renderer.setColorRange(s.device.range);
        renderer.setProjection(s.cloudProjection);renderer.setEyeDomeLighting(s.eyeDomeLighting);return renderer;
    }
    function unavailable(message,retry=false){
        s.cloudUnavailable=message;cloud.stop();view.cloud?.setActive(false);s.cloud.clear();
        if(view.cloud){view.cloud.dispose();view.cloud=null;}
        text('cloud-explanation',message);$('cloud-message').hidden=false;$('cloud-retry').hidden=!retry;
        text('image-state',message);syncPointAppearance();
    }
    function acceptCloud(snapshot,info){
        if(!running||s.device.mode!==4||s.device.frameType!=='depth'||!view.cloud)return;
        const acknowledged=s.device.frame;
        if(acknowledged&&((Number.isSafeInteger(acknowledged.configuration_revision)&&snapshot.revision!==acknowledged.configuration_revision)||
            (acknowledged.units&&snapshot.units!==acknowledged.units)||(acknowledged.units_scope&&snapshot.unitsScope!==acknowledged.units_scope)))throw new Error('Cloud context differs from status; awaiting observational read-back');
        const restart=!!(s.cloud.latest&&s.cloud.reconnecting&&snapshot.frameId<s.cloud.latest.frameId);
        const result=s.cloud.accept(snapshot,info,s.frozenNow??performance.now());
        if(result.ignored)return;
        acceptClipContext(snapshot,restart);
        if(!result.duplicate){if(result.reset)view.cloud.fitted=false;view.cloud.update(snapshot,s.device.range);}
        syncClipping();
        view.schedule(s);
    }
    const cloud=new C.Scheduler({hz:2,read:signal=>source.readCloud({signal}),onValue:acceptCloud,onError:error=>{
        if(error.incompatible){unavailable(error.message);return;}
        s.cloud.error=error.message;s.cloud.reconnecting=true;view.schedule(s);
    }});
    function startFrames(){
        const revision=++representationGeneration;
        frameController?.abort();frameHandle?.stop();frameHandle=null;cloud.stop();view.cloud?.setActive(false);frameController=new AbortController();
        const isCloud=s.device.mode===4;
        for(const id of ['cloud-host','cloud-navigation','cloud-telemetry','reset-cloud','cloud-selection-note'])$(id).hidden=!isCloud;
        view.sensor.hidden=isCloud;$('overlay').hidden=isCloud;view.sensor.tabIndex=isCloud?-1:0;
        $('cloud-legend').hidden=!isCloud||!$('show-overlay').checked;$('cloud-message').hidden=true;
        $('view-mode').querySelector('option[value="4"]').disabled=s.device.frameType!=='depth';
        if(!isCloud){view.cloud?.dispose();view.cloud=null;s.cloud.clear();s.cloudUnavailable=null;}
        syncPointAppearance();
        if(!running){text('image-state','Viewer paused');return;}
        if(isCloud){
            if(s.device.frameType!=='depth'){unavailable(s.device.frameType==='ir'?'3D geometry unavailable for IR. Select Depth deliberately to use 3D.':'3D requires acknowledged Depth acquisition.');return;}
            if(s.cloudUnavailable){unavailable(s.cloudUnavailable,true);return;}
            if(s.device.capture===false){text('image-state','Cloud · capture disabled');return;}
            try{if(!view.cloud)view.cloud=createCloudView();
                view.cloud.mode=$('cloud-nav-mode').value;view.cloud.setActive(true);view.cloud.setOverlay($('show-overlay').checked);syncPointAppearance();cloud.hz=s.cloudHz;cloud.start();}
            catch(error){unavailable('WebGL unavailable: '+error.message,true);}
            return;
        }
        if(s.device.mode==null||s.device.frameType==null){text('image-state','Awaiting acknowledged representation');return;}
        frameHandle=source.startFrames({signal:frameController.signal,fps:()=>Number($('image-rate').value),
            onFrame:(image,m)=>{if(!running||revision!==representationGeneration||s.device.mode===4)return false;return view.drawFrame(image,m,s);},
            onStatus:p=>{if(revision===representationGeneration&&p.capture_enabled===false)text('image-state','Image · capture disabled');},
            onError:error=>{if(revision===representationGeneration)text('image-state','Image delayed · '+error.message);}});
    }
    function syncCloudDownload(){
        const reason=sourceName==='demo'?'Device download is unavailable in synthetic demo.':s.device.mode!==4?'Select 3D to download a point cloud.':s.device.frameType!=='depth'?'Point-cloud download requires acknowledged Depth acquisition.':s.device.capture!==true?'Device capture is unavailable.':null;
        $('obj').disabled=s.objPending||reason!=null;$('obj').setAttribute('aria-busy',String(s.objPending));
        $('obj').title=s.objPending?'Preparing point-cloud download…':reason||'Download fresh device OBJ vertices; XYZ/6000, independent of the displayed camera view.';
    }
    function applyStatus(device,{deterministicCapture=false}={}){
        const key=d=>JSON.stringify([d.mode,d.frameType,d.capture,d.frame?.configuration_revision,d.frame?.units,d.frame?.units_scope]);
        const changed=key(s.device)!==key(device);
        const oldMode=s.device.mode,oldFrame=s.device.frame;
        const restarted=!(sourceName==='demo'&&deterministicCapture)&&Number.isSafeInteger(oldFrame?.frame_id)&&Number.isSafeInteger(device.frame?.frame_id)&&device.frame.frame_id<oldFrame.frame_id;
        s.device=device;if(restarted)s.cloud.reconnecting=true;
        if(device.frame&&Number.isSafeInteger(device.frame.configuration_revision)&&device.frame.units&&device.frame.units_scope){
            acceptClipContext({source:sourceName,frameType:device.frameType,revision:device.frame.configuration_revision,units:device.frame.units,unitsScope:device.frame.units_scope},
                restarted);
        }
        syncCloudDownload();
        $('view-mode').value=device.mode;$('frame-type').value=device.frameType||'';
        if(changed)s.cloudUnavailable=null;
        if(device.range){if(document.activeElement!==$('color-min'))$('color-min').value=device.range.min;if(document.activeElement!==$('color-max'))$('color-max').value=device.range.max;}
        if(changed){
            generation++;measurement.stop();resetLocal('Acknowledged acquisition / mapping change');s.cloud.clear();
            view.cloud?.dispose();view.cloud=null;
            if(device.mode===4&&s.selection.kind==='roi')s.selection={kind:'point',x:Math.min(639,Math.floor((s.selection.x+s.selection.x2)/2)),y:Math.min(479,Math.floor((s.selection.y+s.selection.y2)/2))};
            if((oldMode>=5)!==(device.mode>=5))s.selection={kind:'point',x:device.mode>=5?750:460,y:device.mode>=5?82:320};
            s.hz=s.selection.kind==='roi'?s.roiHz:s.pointHz;measurement.hz=s.hz;syncSelection();view.clearImage();startFrames();if(running)measurement.start();
        }
        view.cloud?.setColorRange(device.range);
        if(!s.mutation)notice('');
        view.schedule(s);
    }
    const status=new C.Scheduler({hz:1,read:signal=>source.readStatus({signal}),onValue:applyStatus,onError:error=>{notice('Status unavailable: '+error.message,true);}});
    const temperature=new C.Scheduler({hz:.25,read:signal=>source.readTemperature({signal}),onValue:p=>text('temperatures',`CPU ${C.format(p.cpu,1)} °C · Vixel ${C.format(p.vixel,1)} °C`),onError:()=>text('temperatures','CPU / Vixel unavailable (read failed)')});
    const metrics=sourceName==='live'?new root.ChitonMetricsReader(p=>text('diagnostics',root.chitonFormatDiagnostics(p))):null;
    let metricsTimer=null;
    function stop(){
        running=false;generation++;representationGeneration++;measurement.stop();status.stop();temperature.stop();cloud.stop();view.cloud?.setActive(false);frameController?.abort();frameHandle?.stop();source.stop();
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
        const moves={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]};if(s.device.mode===4||!moves[event.key]||s.selection.kind!=='point')return;event.preventDefault();
        const [dx,dy]=moves[event.key],step=event.shiftKey?10:1;setSelection({kind:'point',x:Math.max(0,Math.min(view.sensor.width-1,s.selection.x+dx*step)),y:Math.max(0,Math.min(view.sensor.height-1,s.selection.y+dy*step))});
    };
    $('show-overlay').onchange=()=>{if(running)view.drawOverlay(s);view.cloud?.setOverlay($('show-overlay').checked);$('cloud-legend').hidden=s.device.mode!==4||!$('show-overlay').checked;};
    $('reset-cloud').onclick=()=>{view.cloud?.reset();if(running)view.schedule(s);};
    for(const [id,mode] of [['cloud-fit-all','all'],['cloud-fit-main','main']])$(id).onclick=()=>{view.cloud?.fit(mode);if(running)view.schedule(s);};
    $('cloud-orientation').onchange=()=>{if($('cloud-orientation').value)view.cloud?.setOrientation($('cloud-orientation').value);$('cloud-orientation').value='';};
    $('cloud-projection').onchange=()=>{s.cloudProjection=$('cloud-projection').value;view.cloud?.setProjection(s.cloudProjection);if(running)view.schedule(s);};
    $('cloud-edl').onchange=()=>{s.eyeDomeLighting=$('cloud-edl').checked;view.cloud?.setEyeDomeLighting(s.eyeDomeLighting);if(running)view.schedule(s);};
    for(const id of ['point-shape','point-diameter','point-sizing'])$(id).addEventListener(id==='point-diameter'?'input':'change',()=>setPointAppearance({shape:$('point-shape').value,diameterCssPx:Number($('point-diameter').value),sizing:$('point-sizing').value}));
    $('reset-appearance').onclick=()=>setPointAppearance(C.DEFAULT_POINT_STYLE);
    $('clip-form').onsubmit=event=>{
        event.preventDefault();
        s.clipDraft=readClipDraft();
        try{const next=C.validateClipSettings(s.clipDraft);view.cloud?.setClipSettings(next);s.clipSettings=next;s.clipDraft=C.validateClipSettings(next);text('clip-error','');$('clip-error').hidden=true;}
        catch(error){text('clip-error',error.message);$('clip-error').hidden=false;}
        syncClipping();
    };
    $('clip-form').addEventListener('input',()=>{
        s.clipDraft=readClipDraft();
        try{C.validateClipSettings(s.clipDraft);text('clip-error','');$('clip-error').hidden=true;}
        catch(error){text('clip-error',error.message);$('clip-error').hidden=false;}
        syncClipping();
    });
    $('clip-details').addEventListener('toggle',syncClipping);
    $('clip-reset').onclick=()=>resetClipping();
    $('clip-bounds').onclick=()=>{
        const bounds=s.cloud.latest&&C.cloudClipBounds(s.cloud.latest.records);if(!bounds)return;
        bounds.depth.enabled=$('clip-depth-enabled').checked;bounds.box.enabled=$('clip-box-enabled').checked;writeClipDraft(bounds);syncClipping();
    };
    $('cloud-nav-mode').onchange=()=>{if(view.cloud)view.cloud.mode=$('cloud-nav-mode').value;};
    for(const [id,delta] of [['cloud-zoom-in',-.2],['cloud-zoom-out',.2]])$(id).onclick=()=>{if(view.cloud){view.cloud.nav.zoom(delta);view.cloud.dirty();}};
    $('cloud-retry').onclick=()=>{if(!running)return;s.cloudUnavailable=null;view.cloud?.dispose();view.cloud=null;s.cloud.clear();startFrames();};
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
    $('raw').onclick=()=>action(async()=>{const result=await boundedOperation(signal=>source.exportFrame({format:'raw',signal}),5000);download(result.blob,result.filename);});
    $('obj').onclick=()=>action(async()=>{
        if($('obj').disabled)return;s.objPending=true;syncCloudDownload();
        try{
            const result=await boundedOperation(signal=>source.exportFrame({format:'obj',signal}),5000);
            if(result.metadata.type!=='depth')throw new Error('Depth point-cloud download unavailable for IR');
            download(result.blob,result.filename);
        }finally{s.objPending=false;syncCloudDownload();}
    });
    $('view-mode').onchange=()=>action(()=>{const mode=Number($('view-mode').value);if(mode===4&&s.device.frameType!=='depth'){$('view-mode').value=s.device.mode;throw new Error('Select Depth explicitly before entering 3D');}return mutate('mode',mode);});
    $('frame-type').onchange=()=>action(()=>mutate('frameType',$('frame-type').value));
    $('device-settings').onsubmit=e=>{e.preventDefault();action(()=>mutate($('processing-setting').value,$('processing-value').value==='true'));};
    $('range-settings').onsubmit=e=>{e.preventDefault();action(()=>mutate('range',{min:Number($('color-min').value),max:Number($('color-max').value)}));};
    $('quality-settings').onsubmit=e=>{e.preventDefault();action(()=>mutate('quality',Number($('jpeg-quality').value)));};
    $('point-rate').onchange=()=>{s.pointHz=Number($('point-rate').value);if(s.selection.kind==='point'){s.hz=s.pointHz;if(running)measurement.restart(s.hz);else measurement.hz=s.hz;}};
    $('roi-rate').onchange=()=>{s.roiHz=Number($('roi-rate').value);if(s.selection.kind==='roi'){s.hz=s.roiHz;if(running)measurement.restart(s.hz);else measurement.hz=s.hz;}};
    $('image-rate').onchange=()=>{if(running&&s.device.mode!==4)startFrames();};
    $('cloud-rate').onchange=()=>{
        try{
            const next=C.cloudTargetRate($('cloud-rate').value);
            if(running&&cloud.running)cloud.restart(next);else cloud.hz=next;
            s.cloudHz=next;text('cloud-target',next+' updates/s');$('cloud-rate-error').hidden=true;
        }catch(error){$('cloud-rate').value=String(s.cloudHz);text('cloud-rate-error',error.message);$('cloud-rate-error').hidden=false;}
    };
    function preference(persist=true){
        document.body.dataset.size=$('text-size').value;s.precision=Number($('precision').value);if(running)view.schedule(s);
        if(persist)try{localStorage.setItem('chiton-console-display',JSON.stringify({size:$('text-size').value,precision:s.precision}));}catch(_){}
    }
    try{const p=JSON.parse(localStorage.getItem('chiton-console-display'));if(p&&['normal','large','presentation'].includes(p.size))$('text-size').value=p.size;if(p&&[0,1,2].includes(p.precision))$('precision').value=p.precision;}catch(_){}
    $('text-size').onchange=()=>preference();$('precision').onchange=()=>preference();preference(false);
    for(const scenario of C.scenarios){const option=document.createElement('option');option.value=scenario;option.textContent=scenario.replaceAll('-',' ');$('scenario').append(option);}
    $('scenario').value=source.scenario||'stable';
    function reloadScenario(){const url=new URL(location.href);url.searchParams.set('scenario',$('scenario').value);url.searchParams.set('source','demo');location.assign(url);}
    $('scenario').onchange=reloadScenario;$('demo-reset').onclick=reloadScenario;
    $('demo-controls').hidden=sourceName!=='demo';$('raw').disabled=sourceName==='demo';syncCloudDownload();
    text('source-badge',sourceName==='demo'?'DEMO':'LIVE');$('source-badge').setAttribute('aria-label',sourceName==='demo'?'Demo source · synthetic data':'Live source · same-origin API');text('viewport-source',sourceName==='demo'?'DEMO · SYNTHETIC DATA':'LIVE API');
    text('source-description',sourceName==='demo'?'Local synthetic numeric scene. No device APIs are contacted.':'Existing APIs on this page’s origin. Failures never substitute demo data.');
    if(sourceName==='demo'){
        text('device-heading','Simulated device settings');text('device-warning','Simulated only. Color range affects Color/Color panorama, not grayscale. No device API calls are made.');
        $('jpeg-quality').disabled=true;$('quality-settings').querySelector('button').disabled=true;
        const explanation=document.createElement('p');explanation.textContent='JPEG quality is unavailable in demo: this source renders a numeric canvas, not JPEGs.';$('quality-settings').append(explanation);
    }
    $('switch-source').href=sourceName==='demo'?'?source=live':'?source=demo';text('switch-source',sourceName==='demo'?'Switch explicitly to same-origin live APIs →':'Switch explicitly to synthetic demo →');
    document.addEventListener('visibilitychange',()=>{if(document.hidden){stop();s.phase='paused';}else start();});
    root.addEventListener('pagehide',()=>{stop();view.cloud?.dispose();view.cloud=null;for(const url of objectURLs)URL.revokeObjectURL(url);objectURLs.clear();});
    root.addEventListener('pageshow',()=>start());root.addEventListener('resize',()=>{if(running){view.drawOverlay(s);view.cloud?.resize();view.schedule(s);}});
    // Local automation: real source samples pre-rolled at declared observation times, not fabricated API startup history.
    async function prepareCapture({timeMs=6000,filter=false,cloudMode=false,cloudIR=false,pointStyle,eyeDomeLighting=s.eyeDomeLighting,projection=s.cloudProjection,clipSettings}={}){
        if(sourceName!=='demo')throw new Error('Freeze is available only in demo mode');
        const captureStyle=pointStyle===undefined?s.pointStyle:C.validatePointStyle(pointStyle),captureClip=clipSettings===undefined?null:C.validateClipSettings(clipSettings);
        if(typeof eyeDomeLighting!=='boolean'||!['perspective','orthographic'].includes(projection))throw new Error('Invalid capture rendering setting');
        stop();s.eyeDomeLighting=eyeDomeLighting;s.cloudProjection=projection;setPointAppearance(captureStyle);
        view.cloud?.setProjection(projection);view.cloud?.setEyeDomeLighting(eyeDomeLighting);source.seek(0);clientId=C.createClientId();resetLocal('Deterministic review pre-roll');
        if(cloudMode)source.mode=4;if(cloudIR)source.frameType='ir';
        // Explicit demo pre-roll rewinds time; it is not evidence of a process restart.
        // Clear only transport/frame tracking, retaining same-coordinate applied settings and drafts.
        s.cloud.clear();applyStatus(await source.readStatus(),{deterministicCapture:true});
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
        if(s.device.mode===4){
            s.cloudUnavailable=null;
            if(s.device.frameType!=='depth')unavailable('3D geometry unavailable for IR.');
            else try{
                if(!view.cloud)view.cloud=createCloudView();
                syncPointAppearance();view.cloud.setActive(true);view.cloud.setOverlay($('show-overlay').checked);
                const snapshot=await source.readCloud();snapshot.receivedMonoMs=reviewNow;const result=s.cloud.accept(snapshot,{latency:0},reviewNow);
                acceptClipContext(snapshot);
                if(captureClip){s.clipSettings=captureClip;writeClipDraft(captureClip);}
                view.cloud.setClipSettings(s.clipSettings);view.cloud.fitted=!result.reset;view.cloud.update(snapshot,s.device.range);view.cloud.reset();view.cloud.render();syncClipping();
            }catch(error){unavailable(error.message,true);}
        }
        if(s.device.mode!==4&&imageSnapshot)view.drawFrame({synthetic:true,source,metadata:imageSnapshot},{id:imageSnapshot.frame_id,mode:source.mode,age:imageSnapshot.frame_age_ms,reviewReceived:reviewNow-(end-imageTime)},s);
        view.render(s,performance.now());await document.fonts.ready;await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));
        root.CONSOLE_READY=true;return {source:sourceName,scenario:source.scenario,seed:source.seed,timeMs:end,frame:source.metadata().frame_id,cloud:s.device.mode===4?{frameId:s.cloud.latest?.frameId,recordCount:s.cloud.latest?.recordCount,range:view.cloud?.range??null,pointStyle:{...s.pointStyle},renderer:view.cloud?.counters()}:null};
    }
    root.ChitonConsoleApp={state:s,source,view,measurement,status,cloud,start,startFrames,stop,setSelection,prepareCapture,accept,acceptCloud,applyStatus};
    syncSelection();start();root.CONSOLE_READY=true;
})(window);
