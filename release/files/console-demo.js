/* Coherent seeded uint16 scene. Source time, never request count, determines frame ID. */
(function(root){
    'use strict';
    const C=root.ChitonConsole=root.ChitonConsole||{};
    if(typeof module==='object')Object.assign(C,require('./console-model.js'));
    const scenarios=['stable','spikes','step','movement','slow','measurement-failure','image-failure','frozen','restart','contexts','roi','warming','ir','panorama','long-values'];
    function hash(n){n=Math.imul(n^(n>>>16),0x45d9f3b);n=Math.imul(n^(n>>>16),0x45d9f3b);return (n^(n>>>16))>>>0;}
    function pixel(x,y,frame,{seed=7,scenario='stable',frameType='depth',units='mm'}={}) {
        const radius=Math.hypot((x-320)/1.05,y-240);
        if(radius<85||radius>232)return 0;
        const noise=((hash((y*640+x)^Math.imul(frame,101)^seed)%10001)/10000+
            (hash((y*640+x)^Math.imul(frame,997)^seed)%10001)/10000-1)*12;
        let z=1175 + (x<250?420:0) + (y<150?250:0) + Math.round((y-240)*.1);
        if(scenario==='spikes' && frame%60===30)z+=400;
        if(scenario==='step' && frame>=90)z+=500;
        if(scenario==='movement')z+=150*Math.sin(frame/40);
        if(scenario==='long-values')z=65530;
        if(frameType==='ir')z=2200+(x-320)*3;
        else if(units==='depth_sensor_units')z*=2;
        return Math.max(0,Math.min(65535,Math.round(z+noise)));
    }
    function abortableDelay(ms,signal){return new Promise((resolve,reject)=>{
        const abort=()=>{clearTimeout(timer);signal?.removeEventListener('abort',abort);reject(new Error('Cancelled'));};
        const timer=setTimeout(()=>{signal?.removeEventListener('abort',abort);resolve();},ms);
        signal?.addEventListener('abort',abort,{once:true});if(signal?.aborted)abort();
    });}
    class DemoSource {
        constructor({seed=7,scenario='stable',clock=()=>performance.now()}={}){
            if(!scenarios.includes(scenario))throw new Error('Unknown demo scenario');
            this.seed=C.integer(seed,'seed',0,0xffffffff);this.scenario=scenario;this.clock=clock;this.origin=clock();this.frozenTime=null;
            this.mode=scenario==='panorama'?5:1;this.frameType=scenario==='ir'?'ir':'depth';this.calibrated=true;this.smoothed=false;
            this.range={min:0,max:3000};this.revision=1;this.histories=new Map();this.active=0;this.maximum=0;this.framesStop=null;this.mutations=[];
        }
        elapsed(){return this.frozenTime==null?Math.max(0,this.clock()-this.origin):this.frozenTime;}
        seek(ms){this.frozenTime=ms;}
        resume(){this.origin=this.clock()-this.elapsed();this.frozenTime=null;}
        metadata(){
            const t=this.elapsed(),hz=this.scenario==='slow'?5:30;
            let f=Math.floor(t*hz/1000)+1;
            if(this.scenario==='frozen' && t>2000)f=61;
            if(this.scenario==='restart' && t>=11000)f=Math.floor((t-11000)*hz/1000)+1;
            let type=this.frameType,cal=this.calibrated,smooth=this.smoothed,rev=this.revision;
            if(this.scenario==='contexts'){if(t>=4000)type='ir';if(t>=8000){type='depth';cal=false;}if(t>=12000)smooth=true;rev+=Math.floor(t/4000);}
            const units=type==='ir'?'ir_intensity_units':cal?'mm':'depth_sensor_units';
            const acquired=this.scenario==='frozen'&&t>2000?2000:Math.floor(t*hz/1000)*1000/hz;
            return {frame_id:f,captured_at_ms:1700000000000+acquired,frame_age_ms:Math.max(0,t-acquired),configuration_revision:rev,
                color_mode:this.mode,current_mode:this.mode,frame_type:type,units,units_scope:cal&&type==='depth'?'calibration_aoi':'full_sensor',
                calibration_applied:cal&&type==='depth',smoothed:smooth,plugin_applied:false,invalid_sample_policy:'all_uint16_included',
                width:this.mode>=5?1500:640,height:this.mode>=5?165:480};
        }
        sample(x,y,m){
            if(m.width===1500){if(y<8 || y>156)throw new Error('Unmapped panorama pixel');const angle=x/1500*2*Math.PI,r=95+y/165*130;x=Math.round(320+Math.cos(angle)*r*1.05);y=Math.round(240+Math.sin(angle)*r);}
            return pixel(x,y,m.frame_id,{seed:this.seed,scenario:this.scenario,frameType:m.frame_type,units:m.units});
        }
        async readPayload({clientId,selection:s,signal}){
            if(!/^[A-Za-z0-9_-]{1,64}$/.test(clientId))throw new Error('Invalid client identity');
            this.active++;this.maximum=Math.max(this.maximum,this.active);
            try{
                if(this.frozenTime==null && this.scenario==='measurement-failure'){
                    await abortableDelay(150,signal);if(this.elapsed()%12000>4000)throw new Error('Simulated measurement outage');
                }
                if(signal?.aborted)throw new Error('Cancelled');
                if(this.scenario==='restart' && this.elapsed()>=8000 && this.elapsed()<11000)throw new Error('Simulated disconnection');
                const m=this.metadata();C.selection(s,m.width,m.height);
                if(s.kind==='roi' && this.mode>=5)throw new Error('Panorama ROI unsupported');
                const key=JSON.stringify([s,m.configuration_revision,m.units,this.mode,m.smoothed]);
                let h=this.histories.get(clientId),reset=!h||h.key!==key||m.frame_id<h.last||this.elapsed()-h.used>300000;
                if(reset)h={key,items:[],last:0};
                let values,value;
                if(s.kind==='roi'){
                    values=[];for(let y=s.y;y<s.y2;y++)for(let x=s.x;x<s.x2;x++)values.push(this.sample(x,y,m));
                    value=this.sample(Math.floor((s.x+s.x2)/2),Math.floor((s.y+s.y2)/2),m);h.items=[];
                }else{
                    value=this.sample(s.x,s.y,m);
                    if(m.frame_id!==h.last)h.items.push({value,id:m.frame_id,time:m.captured_at_ms});
                    if(h.items.length>60)h.items.shift();values=h.items.map(v=>v.value);
                }
                h.last=m.frame_id;h.used=this.elapsed();this.histories.delete(clientId);this.histories.set(clientId,h);
                for(const [id,history] of this.histories)if(this.histories.size>32||this.elapsed()-history.used>300000)this.histories.delete(id);
                const stats=C.stats(values),trim=values.filter(v=>Math.abs(v-stats.mean)<=stats.stddev);
                return {...m,value,mean:stats.mean,min:stats.min,max:stats.max,stddev:stats.stddev,stdd:stats.stddev*1000,diff:stats.range,
                    sample_count:values.length,statistics_kind:s.kind==='roi'?'spatial':'temporal_observed_frames',history_reset:reset,
                    history_capacity:60,first_captured_at_ms:h.items[0]?.time,last_captured_at_ms:h.items.at(-1)?.time,
                    first_frame_id:h.items[0]?.id,last_frame_id:h.items.at(-1)?.id,
                    rms_about_zero:Math.sqrt(values.reduce((sum,v)=>sum+v*v,0)/values.length),
                    '1std_mean':trim.length?trim.reduce((a,b)=>a+b,0)/trim.length:stats.mean};
            }finally{this.active--;}
        }
        async readMeasurement(args){return C.normalize(await this.readPayload(args),'demo');}
        async readStatus({signal}={}){if(signal?.aborted)throw new Error('Cancelled');const m=this.metadata();return {mode:this.mode,frameType:m.frame_type,capture:true,range:{...this.range},frame:m,cpu:null,vixel:null,simulated:true};}
        async applySetting({name,value,signal}){
            if(signal?.aborted)throw new Error('Cancelled');this.mutations.push(name);
            if(name==='mode')this.mode=C.integer(value,'mode',0,6);
            else if(name==='frameType')this.frameType=C.category(value,['depth','ir'],'type');
            else if(name==='smoothing')this.smoothed=C.boolean(value,'smoothing');
            else if(name==='calibration')this.calibrated=C.boolean(value,'calibration');
            else if(name==='range'){C.integer(value.min,'min',0,65535);C.integer(value.max,'max',value.min+1,65535);this.range={...value};}
            else if(name==='quality')throw new Error('JPEG quality unavailable: the demo renders a numeric canvas, not a JPEG encoder');
            else throw new Error('Unavailable setting');
            this.revision++;return {value,simulated:true};
        }
        frameSnapshot(){
            if(this.scenario==='image-failure' && this.elapsed()>3000)throw new Error('Simulated image outage');
            if(this.scenario==='restart' && this.elapsed()>=8000 && this.elapsed()<11000)throw new Error('Simulated disconnect');
            return this.metadata();
        }
        startFrames({onFrame,onError,onStatus,signal,fps=()=>30}){
            let timer=null,stopped=false,last=0;
            const tick=()=>{
                if(stopped)return;
                try{const m=this.frameSnapshot();if(m.frame_id!==last){last=m.frame_id;onFrame({synthetic:true,source:this,metadata:m},{id:m.frame_id,mode:m.color_mode,age:m.frame_age_ms});}onStatus?.({mode:this.mode,capture_enabled:true});}
                catch(error){onError(error);}
                timer=setTimeout(tick,1000/fps());
            };
            const stop=()=>{stopped=true;clearTimeout(timer);signal?.removeEventListener('abort',stop);};
            signal?.addEventListener('abort',stop,{once:true});if(!signal?.aborted)tick();else stop();
            this.framesStop=stop;return {stop};
        }
        stop(){this.framesStop?.();}
    }
    Object.assign(C,{DemoSource,pixel,scenarios});if(typeof module==='object')module.exports={DemoSource,pixel,scenarios};
})(typeof window==='object'?window:globalThis);
