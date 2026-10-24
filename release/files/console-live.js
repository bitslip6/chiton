/* Existing same-origin APIs only. No startup mutations and no synthetic fallback. */
(function(root){
    'use strict';
    const C=root.ChitonConsole=root.ChitonConsole||{};
    async function limitedBlob(response,limit){
        const declared=response.headers.get('content-length');
        if(declared!=null && (!/^\d+$/.test(declared)||Number(declared)>limit))throw new Error('Response exceeds size limit');
        const reader=response.body.getReader(),chunks=[];let size=0;
        try{for(;;){const {done,value}=await reader.read();if(done)break;size+=value.byteLength;if(size>limit)throw new Error('Response exceeds size limit');chunks.push(value);}}
        catch(error){await reader.cancel().catch(()=>{});throw error;}
        finally{reader.releaseLock();}
        return new Blob(chunks,{type:response.headers.get('content-type')||''});
    }
    async function json(route,signal){
        const r=await fetch(route,{cache:'no-store',signal});
        if(!r.ok)throw new Error('HTTP '+r.status+' · '+route);
        if(!(r.headers.get('content-type')||'').includes('application/json'))throw new Error('Expected JSON');
        return JSON.parse(await (await limitedBlob(r,128*1024)).text());
    }
    class LiveSource {
        constructor(){this.mode=null;this.handles=new Set();}
        async readMeasurement({clientId,selection:s,signal}){
            if(!/^[A-Za-z0-9_-]{1,64}$/.test(clientId))throw new Error('Invalid client ID');
            C.selection(s,this.mode>=5?1500:640,this.mode>=5?165:480);
            if(this.mode>=5 && s.kind==='roi')throw new Error('Panorama ROI unsupported');
            const coordinates=s.kind==='roi'?[s.x,s.y,s.x2,s.y2]:[s.x,s.y];
            return C.normalize(await json('/get/value/client/'+encodeURIComponent(clientId)+'/'+coordinates.join('/'),signal),'live');
        }
        async readStatus({signal}){
            const mode=await json('/colormode/setting',signal);
            C.integer(mode.value,'mode',0,6);C.boolean(mode.capture_enabled,'capture');
            this.mode=mode.value;
            const optional=async route=>{try{return await json(route,signal);}catch(e){if(signal?.aborted)throw e;return null;}};
            const [depth,range,frame,dimensions]=await Promise.all(['/depth/setting','/range/setting','/frame/camera','/dimension/camera'].map(optional));
            let frameType=null,validRange=null;
            if(depth){C.boolean(depth.value,'depth');C.category(depth.frame_type,['depth','ir'],'type');if(depth.value!==(depth.frame_type==='depth'))throw new Error('Inconsistent acquisition state');frameType=depth.frame_type;}
            if(range){C.integer(range.min,'range min',0,65535);C.integer(range.max,'range max',range.min+1,65535);validRange=range;}
            if(dimensions)for(const k of ['inner_dist','outer_dist','center_x','center_y'])C.integer(dimensions[k],k,0,10000);
            return {mode:mode.value,capture:mode.capture_enabled,frameType,range:validRange,frame,dimensions};
        }
        async readCloud({signal}={}){
            if(this.mode!==4)throw new Error('Cloud requires acknowledged mode 4');
            return C.readCloudResponse(await fetch('/pointcloud/camera',{cache:'no-store',signal}),signal);
        }
        async readTemperature({signal}){const p=await json('/temp/setting',signal);return {cpu:C.finite(p.cputemp,'CPU temperature'),vixel:C.finite(p.vixeltemp,'Vixel temperature')};}
        async applySetting({name,value,signal}){
            let route;
            if(name==='mode')route='/colormode/setting/'+C.integer(value,'mode',0,6);
            else if(name==='frameType')route='/depth/setting/'+C.category(value,['depth','ir'],'type');
            else if(name==='range'){C.integer(value.min,'min',0,65535);C.integer(value.max,'max',value.min+1,65535);route='/range/setting/'+value.min+'/'+value.max;}
            else if(name==='smoothing'||name==='calibration'){C.boolean(value,'toggle');route='/'+(name==='smoothing'?'smoothing':'chitoncal')+'/setting/'+(value?'on':'off');}
            else if(name==='quality')route='/quality/setting/'+C.integer(value,'quality',1,100);
            else throw new Error('Unsupported setting');
            const ack=await json(route,signal);
            if(name==='range'){if(ack.min!==value.min||ack.max!==value.max)throw new Error('Unexpected acknowledgment');}
            else if(name==='frameType'){if(ack.frame_type!==value || ack.value!==(value==='depth'))throw new Error('Unexpected acknowledgment');}
            else if(ack.value!==value)throw new Error('Unexpected acknowledgment; outcome unknown');
            return ack;
        }
        startFrames({onFrame,onStatus,onError,signal,fps=()=>30}){
            let stream=null,stopped=false;
            const loop=new C.Scheduler({hz:fps(),read:async s=>{
                loop.hz=fps();
                if(this.mode===4){onStatus({mode:4,capture_enabled:true,unsupported:true});return null;}
                if(this.mode==null)return null;
                if(stream?.socket || stream?.start())return null;
                const response=await fetch('/image/camera',{cache:'no-store',signal:s});
                if(!response.ok)throw new Error('JPEG HTTP '+response.status);
                if(!(response.headers.get('content-type')||'').startsWith('image/jpeg'))throw new Error('Expected JPEG');
                const h=n=>response.headers.get('X-Chiton-'+n);
                const id=C.integer(Number(h('Frame-Id')),'JPEG frame ID',1),mode=C.integer(Number(h('Color-Mode')),'JPEG mode',0,6);
                const age=h('Frame-Age-Ms')==null?null:C.finite(Number(h('Frame-Age-Ms')),'JPEG age',0);
                const blob=await limitedBlob(response,2*1024*1024);
                const image=await createImageBitmap(blob);
                if(s.aborted || stopped){image.close();throw new Error('Cancelled');}
                if(!((mode<5 && image.width===640 && image.height===480)||(mode>=5 && image.width===1500 && image.height===165))){image.close();throw new Error('Unexpected JPEG dimensions');}
                try{onFrame(image,{id,mode,age});}finally{image.close();}return null;
            },onError});
            if(typeof root.ChitonImageStream==='function')stream=new root.ChitonImageStream({fps,onFrame:(image,m)=>{if(stopped||this.mode===4)return false;return onFrame(image,m);},onStatus,onError});
            const stop=()=>{stopped=true;stream?.stop();loop.stop();signal?.removeEventListener('abort',stop);this.handles.delete(handle);};
            const handle={stop,stream,loop};this.handles.add(handle);signal?.addEventListener('abort',stop,{once:true});
            if(!signal?.aborted)loop.start();else stop();return handle;
        }
        async exportFrame({format,signal}){
            const raw=format==='raw';if(!raw && format!=='obj')throw new Error('Unsupported export');
            const response=await fetch(raw?'/raw/camera':'/pointcloud_obj/camera',{cache:'no-store',signal});
            if(!response.ok)throw new Error('Export HTTP '+response.status);
            const expected=raw?'processed_uint16_row_major':'obj_xyz_divided_by_6000';
            if(response.headers.get('X-Chiton-Layout')!==expected ||
                !(response.headers.get('content-type')||'').startsWith(raw?'application/octet-stream':'text/plain'))throw new Error('Export metadata/type mismatch');
            const id=C.integer(Number(response.headers.get('X-Chiton-Frame-Id')),'export frame',1);
            const type=C.category(response.headers.get('X-Chiton-Frame-Type'),['depth','ir'],'export type');
            const units=response.headers.get('X-Chiton-Source-Units');C.category(units,['mm','depth_sensor_units','ir_intensity_units'],'export units');
            if((type==='ir')!==(units==='ir_intensity_units'))throw new Error('Inconsistent export units/type');
            if(response.headers.get('X-Chiton-Units')!==(raw?units:'normalized_6000_'+units))throw new Error('Export unit representation mismatch');
            C.category(response.headers.get('X-Chiton-Units-Scope'),['full_sensor','calibration_aoi'],'export scope');
            C.integer(Number(response.headers.get('X-Chiton-Configuration-Revision')),'export revision');
            if(raw)C.category(response.headers.get('X-Chiton-Byte-Order'),['little','big'],'export byte order');
            if(raw && (response.headers.get('X-Chiton-Width')!=='640'||response.headers.get('X-Chiton-Height')!=='480'))throw new Error('Raw dimensions mismatch');
            const declared=Number(response.headers.get('content-length'));if(declared>24*1024*1024)throw new Error('Export exceeds limit');
            const blob=await limitedBlob(response,raw?614400:24*1024*1024);if(raw && blob.size!==614400)throw new Error('Invalid export size');
            return {blob,filename:raw?'chiton-raw.bin':'chiton-pointcloud.obj',metadata:{id,type,units,source:'live'}};
        }
        stop(){for(const handle of [...this.handles])handle.stop();}
    }
    Object.assign(C,{LiveSource});if(typeof module==='object')module.exports={LiveSource};
})(typeof window==='object'?window:globalThis);
