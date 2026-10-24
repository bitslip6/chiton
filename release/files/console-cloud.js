/* Existing int16 XYZD wire contract and pure viewer math. API axes are never mirrored. */
(function(root){
    'use strict';
    const C=root.ChitonConsole=root.ChitonConsole||{};
    if(typeof module==='object')Object.assign(C,require('./console-model.js'));
    const CLOUD_MAX=640*480,CLOUD_BYTES=CLOUD_MAX*8;
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
    function cloudBounds(records){
        if(!(records instanceof Int16Array)||records.length%4||records.length>CLOUD_MAX*4)throw new Error('Invalid signed cloud records');
        const min=[Infinity,Infinity,Infinity],max=[-Infinity,-Infinity,-Infinity];
        for(let i=0;i<records.length;i+=4)for(let k=0;k<3;k++){min[k]=Math.min(min[k],records[i+k]);max[k]=Math.max(max[k],records[i+k]);}
        if(!records.length)return {center:[0,0,0],radius:1,min:[0,0,0],max:[0,0,0],empty:true};
        const center=min.map((v,k)=>(v+max[k])/2),radius=Math.max(1,Math.hypot(...max.map((v,k)=>(v-min[k])/2)));
        return {center,radius,min,max,empty:false};
    }
    function cloudColor(depth,range,out=[0,0,0],offset=0){
        if(!range||!Number.isFinite(range.min)||!Number.isFinite(range.max)||range.max<=range.min){out[offset]=.6;out[offset+1]=.67;out[offset+2]=.73;return out;}
        const t=Math.max(0,Math.min(1,(depth-range.min)/(range.max-range.min)));
        out[offset]=Math.max(0,1-Math.abs(t*4-3));out[offset+1]=Math.max(0,1-Math.abs(t*4-2));out[offset+2]=Math.max(0,1-Math.abs(t*4-1));return out;
    }
    const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
    class CloudCamera {
        constructor(){this.bounds=cloudBounds(new Int16Array());this.home();}
        home(){this.theta=.35;this.phi=1.25;this.fit();}
        fit(aspect=this.aspect||1){this.aspect=clamp(aspect,.05,20);this.target=[...this.bounds.center];
            const half=Math.min(Math.PI/8,Math.atan(Math.tan(Math.PI/8)*this.aspect));
            this.radius=clamp(this.bounds.radius/Math.sin(half)*1.15,1,1e7);this.base=this.radius;}
        orbit(dx,dy){C.finite(dx,'orbit X');C.finite(dy,'orbit Y');this.theta=(this.theta+clamp(dx,-2,2))%(2*Math.PI);this.phi=clamp(this.phi+clamp(dy,-2,2),.05,Math.PI-.05);}
        zoom(delta){C.finite(delta,'zoom');this.radius=clamp(this.radius*Math.exp(clamp(delta,-2,2)),Math.max(.1,this.base*.05),this.base*20);}
        pan(dx,dy){C.finite(dx,'pan X');C.finite(dy,'pan Y');const k=this.radius*.002,right=[Math.cos(this.theta),0,-Math.sin(this.theta)],up=[-Math.cos(this.phi)*Math.sin(this.theta),Math.sin(this.phi),-Math.cos(this.phi)*Math.cos(this.theta)];
            this.target=this.target.map((v,i)=>clamp(v+(-right[i]*clamp(dx,-1000,1000)+up[i]*clamp(dy,-1000,1000))*k,this.bounds.center[i]-this.base*3,this.bounds.center[i]+this.base*3));}
        pose(){return {target:[...this.target],radius:this.radius,theta:this.theta,phi:this.phi};}
        position(){return this.target.map((v,i)=>v+this.radius*[Math.sin(this.phi)*Math.sin(this.theta),Math.cos(this.phi),Math.sin(this.phi)*Math.cos(this.theta)][i]);}
    }
    class CloudTracker {
        constructor(){this.clear();}
        clear(){this.latest=null;this.times=[];this.context=null;this.reconnecting=false;this.error=null;this.latency=null;}
        accept(m,info={latency:0},now=performance.now()){
            const context=JSON.stringify([m.source,m.revision,m.frameType,m.units,m.unitsScope]);
            const reset=context!==this.context||(this.latest&&m.frameId<this.latest.frameId&&this.reconnecting);
            if(!reset&&this.latest&&m.frameId<this.latest.frameId)return {ignored:true};
            if(reset){this.clear();this.context=context;}
            this.error=null;this.reconnecting=false;this.latency=info.latency;
            if(this.latest?.frameId===m.frameId){this.latest={...this.latest,frameAgeMs:m.frameAgeMs,receivedMonoMs:m.receivedMonoMs,bytes:m.bytes};return {duplicate:true};}
            this.latest={...m,distinctReceived:now};this.times.push(now);this.times=this.times.filter(t=>t>=now-2000).slice(-60);
            return {reset};
        }
        health(now,hz){const m=this.latest;if(!m)return this.error?'Cloud unavailable':'Cloud connecting';
            const stale=this.error||now-m.distinctReceived>Math.max(1000,3000/hz)||(m.frameAgeMs!=null&&m.frameAgeMs+now-m.receivedMonoMs>Math.max(1000,3000/hz));
            return (m.recordCount?'Cloud':'Empty cloud')+(stale?' · stale':' · live');}
        hz(now){const t=this.times.filter(v=>v>=now-2000);return t.length>1?(t.length-1)*1000/Math.max(1,now-t[0]):0;}
    }
    Object.assign(C,{CLOUD_MAX,CLOUD_BYTES,cloudHeaders,decodeCloud,readCloudResponse,cloudBounds,cloudColor,CloudCamera,CloudTracker});
    if(typeof module==='object')module.exports=C;
})(typeof window==='object'?window:globalThis);
