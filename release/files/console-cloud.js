/* Existing int16 XYZD wire contract and pure viewer math. API axes are never mirrored. */
(function(root){
    'use strict';
    const C=root.ChitonConsole=root.ChitonConsole||{};
    if(typeof module==='object')Object.assign(C,require('./console-model.js'));
    const CLOUD_MAX=640*480,CLOUD_BYTES=CLOUD_MAX*8;
    function cloudTargetRate(value){
        // Validate before any controller/Scheduler state changes, including inactive paths.
        if(typeof value==='string'&&!/^(?:\d+)(?:\.\d+)?$/.test(value))throw new Error('Cloud target must be greater than 0 and at most 30 updates/s');
        const hz=typeof value==='string'?Number(value):value;
        if(typeof hz!=='number'||!Number.isFinite(hz)||hz<=0||hz>30)throw new Error('Cloud target must be greater than 0 and at most 30 updates/s');
        return hz;
    }
    function cloudHeaders(headers){
        const h=n=>headers.get('X-Chiton-'+n);
        const number=(name,min=0,max=Number.MAX_SAFE_INTEGER,optional=false)=>{
            const v=h(name);if(v==null&&optional)return null;
            if(v==null||!/^\d+$/.test(v))throw new Error('Missing/invalid cloud '+name);
            return C.integer(Number(v),name,min,max);
        };
        if((headers.get('content-type')||'').split(';')[0].trim().toLowerCase()!=='application/octet-stream')throw new Error('Expected binary cloud');
        if(h('Layout')!=='int16_xyzd'||h('Color-Mode')!=='4')throw new Error('Incompatible cloud layout/mode');
        const frameType=C.category(h('Frame-Type'),['depth','ir'],'cloud type');
        if(frameType!=='depth'){const e=new Error('3D geometry unavailable for IR');e.incompatible=true;throw e;}
        const units=C.category(h('Source-Units'),['mm','depth_sensor_units'],'cloud source units');
        if(h('Units')!==units)throw new Error('Cloud unit representation mismatch');
        const unitsScope=C.category(h('Units-Scope'),['calibration_aoi','full_sensor'],'cloud scope');
        if((units==='mm')!==(unitsScope==='calibration_aoi'))throw new Error('Inconsistent cloud units/scope');
        if(h('Invalid-Sample-Policy')!=='all_uint16_included')throw new Error('Unknown cloud sample policy');
        const byteOrder=C.category(h('Byte-Order'),['little','big'],'cloud byte order');
        const recordCount=number('Width',0,CLOUD_MAX);
        if(number('Height',1,1)!==1)throw new Error('Invalid cloud height');
        const length=headers.get('content-length');
        if(length!=null&&(!/^\d+$/.test(length)||!Number.isSafeInteger(Number(length))||Number(length)!==recordCount*8))throw new Error('Cloud length/count mismatch');
        return {frameId:number('Frame-Id',1),revision:number('Configuration-Revision'),frameType,units,unitsScope,byteOrder,recordCount,
            capturedAtMs:number('Captured-At-Ms',1,Number.MAX_SAFE_INTEGER,true),frameAgeMs:number('Frame-Age-Ms',0,Number.MAX_SAFE_INTEGER,true)};
    }
    function decodeCloud(bytes,metadata,source='live',receivedMonoMs=performance.now()){
        const count=C.integer(metadata.recordCount,'cloud count',0,CLOUD_MAX);
        if(!(bytes instanceof ArrayBuffer)||bytes.byteLength!==count*8)throw new Error('Truncated/misaligned cloud');
        const data=new DataView(bytes),records=new Int16Array(count*4),little=metadata.byteOrder==='little';
        C.category(metadata.byteOrder,['little','big'],'cloud byte order');
        for(let i=0;i<records.length;i++)records[i]=data.getInt16(i*2,little);
        return {...metadata,source,receivedMonoMs,records,bytes:bytes.byteLength};
    }
    async function readCloudResponse(response,signal){
        let metadata;
        try{if(!response.ok)throw new Error('Cloud HTTP '+response.status);metadata=cloudHeaders(response.headers);}
        catch(error){await response.body?.cancel().catch(()=>{});throw error;}
        const size=metadata.recordCount*8;
        // Allocate only after validated bounded headers. Stream count is bounded even without Content-Length.
        const bytes=new Uint8Array(size),reader=response.body.getReader();let offset=0;
        try{
            for(;;){if(signal?.aborted)throw new Error('Cloud cancelled');const {done,value}=await reader.read();if(done)break;
                if(offset+value.byteLength>size)throw new Error('Oversized cloud body');bytes.set(value,offset);offset+=value.byteLength;}
            if(offset!==size)throw new Error('Truncated cloud body');
        }catch(error){await reader.cancel().catch(()=>{});throw error;}finally{reader.releaseLock();}
        if(signal?.aborted)throw new Error('Cloud cancelled');return decodeCloud(bytes.buffer,metadata);
    }
    const CLEANUP_FIELDS=['enabled','algorithm','evidence_min','evidence_max','boundary_gap','anchor_tolerance','center_margin','ramp_residual','surface_support','center_support'];
    const CLEANUP_INACTIVE=['anchor_tolerance','ramp_residual','surface_support','center_support'];
    function cleanupDescription(config){return config?.algorithm===3?'Aggressive boundary bands · radius 6 · any opposite pair · ramps/thin surfaces may be lost · not sensor confidence':'Conservative boundary classification · not sensor confidence';}
    const CLEANUP_STATUS=['Not initialized','Off','Ready','Unsupported source context','Invalid configuration','Topology changed — restart required','Unsafe topology'];
    function validateCleanupConfig(input){
        if(!input)throw new Error('Missing cleanup configuration');
        const c={};for(const k of CLEANUP_FIELDS)c[k]=C.integer(input[k],k,0,k==='enabled'?1:k==='algorithm'?3:k==='surface_support'||k==='center_support'?255:65535);
        if(![1,2,3].includes(c.algorithm))throw new Error('Unknown cleanup algorithm');
        return c;
    }
    function validateCleanupSettings(s){
        const advertised=s.constraints?.algorithm??[1,2];
        if(!Array.isArray(advertised)||advertised.some(v=>!Number.isInteger(v)||v<1))throw new Error('Invalid cleanup capabilities');
        const algorithms=advertised.filter(v=>[1,2,3].includes(v));
        if(!algorithms.includes(s.config?.algorithm))throw new Error('Cleanup algorithm not advertised');
        return {...s,algorithms,generation:C.integer(s.generation,'cleanup generation',1),config:validateCleanupConfig(s.config),
            available:C.boolean(s.available,'cleanup availability'),status:C.integer(s.status,'cleanup status',0,6),frame_generation:C.integer(s.frame_generation,'applied generation',0)};
    }
    function decodeCleanup(bytes,receivedMonoMs=performance.now()){
        if(!(bytes instanceof ArrayBuffer)||bytes.byteLength<128)throw new Error('Truncated cleanup envelope');
        const d=new DataView(bytes),u8=new Uint8Array(bytes),u32=o=>d.getUint32(o,true),u64=o=>C.integer(u32(o)+4294967296*u32(o+4),'cleanup identity',0);
        if(String.fromCharCode(...u8.subarray(0,8))!=='CHCLD001'||u32(8)!==128)throw new Error('Unknown cleanup wire version');
        const n=C.integer(u32(12),'cleanup records',0,230400);
        if(u32(16)!==n*8||u32(20)!==n||bytes.byteLength!==128+n*9)throw new Error('Cleanup count/length mismatch');
        const status=C.integer(u32(60),'cleanup status',0,6),algorithm=C.integer(u32(56),'cleanup algorithm',1,3);
        if(u8[66]!==1){const e=new Error('3D geometry unavailable for IR');e.incompatible=true;throw e;}
        if(![1,2].includes(u8[64])||![0,1].includes(u8[65])||(u8[64]===1)!==(u8[65]===1)||![1,2].includes(u8[71]))throw new Error('Unsupported cleanup units/scope/order');
        for(const o of [67,68,69,70])C.integer(u8[o],'cleanup boolean',0,1);
        const config=validateCleanupConfig({enabled:u8[70],algorithm,evidence_min:d.getUint16(72,true),evidence_max:d.getUint16(74,true),
            boundary_gap:d.getUint16(76,true),anchor_tolerance:d.getUint16(78,true),center_margin:d.getUint16(80,true),ramp_residual:d.getUint16(82,true),surface_support:u8[84],center_support:u8[85]});
        const records=new Int16Array(n*4),little=u8[71]===1;
        for(let i=0;i<records.length;i++)records[i]=d.getInt16(128+i*2,little);
        const flags=new Uint8Array(bytes,128+n*8,n),counts={0:0,1:0,2:0,4:0,8:0};
        for(const f of flags){if(!(f in counts))throw new Error('Unknown cleanup flag');counts[f]++;}
        const gated=u32(88),mixed=u32(92),boundary=u32(96),unclassified=u32(100),retained=u32(104),detailed=u32(108),early=u32(112);
        if(counts[8]!==mixed||counts[2]!==boundary||counts[4]!==unclassified||counts[1]!==retained||gated>n||detailed+early!==gated||
            (status===1?counts[0]!==n||config.enabled:counts[0]!==0)||(status!==1&&status!==2&&counts[4]!==n)||mixed+boundary>detailed||(algorithm!==2&&early!==0)||
            (status===2&&(!config.enabled||u8[64]!==1||u8[68]||u8[69])))throw new Error('Inconsistent cleanup metadata');
        return {source:'live',frameId:C.integer(u64(24),'cloud frame',1),revision:u64(32),capturedAtMs:u64(40),frameAgeMs:null,
            frameType:'depth',units:u8[64]===1?'mm':'depth_sensor_units',unitsScope:u8[65]?'calibration_aoi':'full_sensor',
            byteOrder:little?'little':'big',recordCount:n,records,receivedMonoMs,bytes:bytes.byteLength,
            cleanup:{generation:C.integer(u64(48),'cleanup generation',1),status,config,flags,gated,mixed,boundary,unclassified,retained,detailed,early}};
    }
    async function readCleanupResponse(response,signal){
        const max=128+230400*9;
        if(!response.ok){await response.body?.cancel().catch(()=>{});throw new Error('Cleanup cloud HTTP '+response.status);}
        const length=response.headers.get('content-length');
        if(!(response.headers.get('content-type')||'').startsWith('application/octet-stream')||length==null||!/^\d+$/.test(length)||Number(length)<128||Number(length)>max){await response.body?.cancel().catch(()=>{});throw new Error('Invalid cleanup response headers');}
        const bytes=new Uint8Array(Number(length)),reader=response.body.getReader();let offset=0;
        try{for(;;){if(signal?.aborted)throw new Error('Cloud cancelled');const {done,value}=await reader.read();if(done)break;
            if(offset+value.byteLength>bytes.length)throw new Error('Oversized cleanup response');bytes.set(value,offset);offset+=value.byteLength;}
            if(offset!==bytes.length||signal?.aborted)throw new Error('Truncated/cancelled cleanup response');
        }catch(e){await reader.cancel().catch(()=>{});throw e;}finally{reader.releaseLock();}
        return decodeCleanup(bytes.buffer);
    }
    function cloudBounds(records){
        if(!(records instanceof Int16Array)||records.length%4||records.length>CLOUD_MAX*4)throw new Error('Invalid signed cloud records');
        const min=[Infinity,Infinity,Infinity],max=[-Infinity,-Infinity,-Infinity];
        for(let i=0;i<records.length;i+=4)for(let k=0;k<3;k++){min[k]=Math.min(min[k],records[i+k]);max[k]=Math.max(max[k],records[i+k]);}
        if(!records.length)return {center:[0,0,0],radius:1,min:[0,0,0],max:[0,0,0],empty:true};
        const center=min.map((v,k)=>(v+max[k])/2),radius=Math.max(1,Math.hypot(...max.map((v,k)=>(v-min[k])/2)));
        return {center,radius,min,max,empty:false};
    }
    // Camera-only robust bounds: central 98% on each signed XYZ axis. Never filters uploaded records.
    function cloudMainBounds(records){
        const all=cloudBounds(records),count=records.length/4;
        if(count<100)return {...all,included:count,total:count};
        const min=[],max=[],trim=Math.floor(count*.01);
        for(let axis=0;axis<3;axis++){
            const bins=new Uint32Array(65536);
            for(let i=axis;i<records.length;i+=4)bins[records[i]+32768]++;
            let sum=0,lo=null,hi=0;
            for(let i=0;i<bins.length;i++){sum+=bins[i];if(lo===null&&sum>trim)lo=i-32768;if(sum>=count-trim){hi=i-32768;break;}}
            min.push(lo);max.push(hi);
        }
        let included=0;
        for(let i=0;i<records.length;i+=4)if(min.every((v,k)=>records[i+k]>=v&&records[i+k]<=max[k]))included++;
        const center=min.map((v,k)=>(v+max[k])/2),radius=Math.max(1,Math.hypot(...max.map((v,k)=>(v-min[k])/2)));
        return {min,max,center,radius,empty:false,included,total:count};
    }
    function defaultClipSettings(){return {depth:{enabled:false,min:-32768,max:32767},box:{enabled:false,min:[-32768,-32768,-32768],max:[32767,32767,32767]}};}
    function validateClipSettings(input){
        const fail=()=>{throw new Error('Clipping requires complete boolean flags and ordered signed-int16 integer limits');};
        const integer=v=>Number.isInteger(v)&&v>=-32768&&v<=32767;
        if(!input||!input.depth||!input.box)fail();
        const d=input.depth,b=input.box;
        if(typeof d.enabled!=='boolean'||typeof b.enabled!=='boolean'||!integer(d.min)||!integer(d.max)||d.min>d.max||
            !Array.isArray(b.min)||!Array.isArray(b.max)||b.min.length!==3||b.max.length!==3)fail();
        for(let k=0;k<3;k++)if(!integer(b.min[k])||!integer(b.max[k])||b.min[k]>b.max[k])fail();
        return {depth:{enabled:d.enabled,min:d.min,max:d.max},box:{enabled:b.enabled,min:[...b.min],max:[...b.max]}};
    }
    function clippingActive(settings){return settings.depth.enabled||settings.box.enabled;}
    // Internal predicate takes validated settings, avoiding validation/allocations per record.
    function recordMatchesClipping(records,i,s){
        const d=s.depth,b=s.box;
        return (!d.enabled||(records[i+3]>=d.min&&records[i+3]<=d.max))&&
            (!b.enabled||(records[i]>=b.min[0]&&records[i]<=b.max[0]&&records[i+1]>=b.min[1]&&records[i+1]<=b.max[1]&&records[i+2]>=b.min[2]&&records[i+2]<=b.max[2]));
    }
    function validateClipRecords(records){
        if(!(records instanceof Int16Array)||records.length%4||records.length>CLOUD_MAX*4)throw new Error('Invalid signed cloud records');
    }
    function summarizeCloudClipping(records,settings){
        validateClipRecords(records);const s=validateClipSettings(settings),submittedCount=records.length/4;
        let matchingCount=submittedCount;
        if(clippingActive(s)){matchingCount=0;for(let i=0;i<records.length;i+=4)if(recordMatchesClipping(records,i,s))matchingCount++;}
        return {matchingCount,clippedCount:submittedCount-matchingCount,submittedCount};
    }
    // Explicit draft seeding only: empty clouds have no observed extrema.
    function cloudClipBounds(records){
        validateClipRecords(records);if(!records.length)return null;
        const s=defaultClipSettings();s.depth.min=32767;s.depth.max=-32768;s.box.min.fill(32767);s.box.max.fill(-32768);
        for(let i=0;i<records.length;i+=4){s.depth.min=Math.min(s.depth.min,records[i+3]);s.depth.max=Math.max(s.depth.max,records[i+3]);
            for(let k=0;k<3;k++){s.box.min[k]=Math.min(s.box.min[k],records[i+k]);s.box.max[k]=Math.max(s.box.max[k],records[i+k]);}}
        return s;
    }
    function cloudClipContext(m){return JSON.stringify([m.source,m.frameType,m.revision,m.units,m.unitsScope]);}
    function clipDescription(settings){
        const d=settings.depth,b=settings.box,parts=[];
        if(d.enabled)parts.push(`D [${d.min}, ${d.max}]`);
        if(b.enabled)for(let k=0;k<3;k++)parts.push(`${'XYZ'[k]} [${b.min[k]}, ${b.max[k]}]`);
        return parts.length?parts:['Clipping off'];
    }
    function cloudColor(depth,range,out=[0,0,0],offset=0,reverse=false){
        if(!range||!Number.isFinite(range.min)||!Number.isFinite(range.max)||range.max<=range.min){out[offset]=.6;out[offset+1]=.67;out[offset+2]=.73;return out;}
        const normalized=Math.max(0,Math.min(1,(depth-range.min)/(range.max-range.min))),t=reverse?.75-.5*normalized:normalized;
        out[offset]=Math.max(0,1-Math.abs(t*4-3));out[offset+1]=Math.max(0,1-Math.abs(t*4-2));out[offset+2]=Math.max(0,1-Math.abs(t*4-1));return out;
    }
    const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
    class CloudCamera {
        constructor(){this.bounds=cloudBounds(new Int16Array());this.home();}
        home(){this.theta=.35;this.phi=1.25;this.fit();}
        fit(aspect=this.aspect||1,bounds=this.bounds){this.aspect=clamp(aspect,.05,20);this.target=[...bounds.center];this.panAnchor=[...bounds.center];
            const half=Math.min(Math.PI/8,Math.atan(Math.tan(Math.PI/8)*this.aspect));
            this.radius=clamp(bounds.radius/Math.sin(half)*1.15,1,1e7);this.base=this.radius;}
        orient(view){
            const poses={front:[0,Math.PI/2],top:[0,.0001],side:[Math.PI/2,Math.PI/2]};
            if(!poses[view])throw new Error('Invalid API view');[this.theta,this.phi]=poses[view];
        }
        orbit(dx,dy){C.finite(dx,'orbit X');C.finite(dy,'orbit Y');this.theta=(this.theta+clamp(dx,-2,2))%(2*Math.PI);this.phi=clamp(this.phi+clamp(dy,-2,2),.05,Math.PI-.05);}
        zoom(delta){C.finite(delta,'zoom');this.radius=clamp(this.radius*Math.exp(clamp(delta,-2,2)),Math.max(.1,this.base*.05),this.base*20);}
        pan(dx,dy){C.finite(dx,'pan X');C.finite(dy,'pan Y');const k=this.radius*.002,right=[Math.cos(this.theta),0,-Math.sin(this.theta)],up=[-Math.cos(this.phi)*Math.sin(this.theta),Math.sin(this.phi),-Math.cos(this.phi)*Math.cos(this.theta)];
            this.target=this.target.map((v,i)=>clamp(v+(-right[i]*clamp(dx,-1000,1000)+up[i]*clamp(dy,-1000,1000))*k,this.panAnchor[i]-this.base*3,this.panAnchor[i]+this.base*3));}
        pose(){return {target:[...this.target],radius:this.radius,theta:this.theta,phi:this.phi};}
        position(){return this.target.map((v,i)=>v+this.radius*[Math.sin(this.phi)*Math.sin(this.theta),Math.cos(this.phi),Math.sin(this.phi)*Math.cos(this.theta)][i]);}
    }
    class CloudTracker {
        constructor(){this.clear();}
        clear(){this.latest=null;this.times=[];this.context=null;this.reconnecting=false;this.error=null;this.latency=null;}
        accept(m,info={latency:0},now=performance.now()){
            const context=cloudClipContext(m);
            const reset=context!==this.context||(this.latest&&m.frameId<this.latest.frameId&&this.reconnecting);
            if(!reset&&this.latest&&m.frameId<this.latest.frameId)return {ignored:true};
            if(reset){this.clear();this.context=context;}
            this.error=null;this.reconnecting=false;this.latency=info.latency;
            if(this.latest?.frameId===m.frameId){this.latest={...this.latest,frameAgeMs:m.frameAgeMs,receivedMonoMs:m.receivedMonoMs,bytes:m.bytes};return {duplicate:true};}
            this.latest={...m,distinctReceived:now};this.times.push(now);this.times=this.times.filter(t=>t>=now-2000).slice(-64);
            return {reset};
        }
        health(now,hz){const m=this.latest;if(!m)return this.error?'Cloud unavailable':'Cloud connecting';
            const stale=this.error||now-m.distinctReceived>Math.max(1000,3000/hz)||(m.frameAgeMs!=null&&m.frameAgeMs+now-m.receivedMonoMs>Math.max(1000,3000/hz));
            return (m.recordCount?'Cloud':'Empty cloud')+(stale?' · stale':' · live');}
        hz(now){const t=this.times.filter(v=>v>=now-2000);return t.length>1?(t.length-1)*1000/Math.max(1,now-t[0]):0;}
    }
    Object.assign(C,{CLEANUP_FIELDS,CLEANUP_INACTIVE,cleanupDescription,CLEANUP_STATUS,validateCleanupConfig,validateCleanupSettings,decodeCleanup,readCleanupResponse,recordMatchesClipping,cloudTargetRate,defaultClipSettings,validateClipSettings,clippingActive,summarizeCloudClipping,cloudClipBounds,cloudClipContext,clipDescription,CLOUD_MAX,CLOUD_BYTES,cloudHeaders,decodeCloud,readCloudResponse,cloudBounds,cloudMainBounds,cloudColor,CloudCamera,CloudTracker});
    if(typeof module==='object')module.exports=C;
})(typeof window==='object'?window:globalThis);
