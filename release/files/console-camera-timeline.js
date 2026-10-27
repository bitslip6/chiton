/* Bounded viewer-only orbit/target/dolly keyframes. No DOM, transport or timers. */
(function(root){
    'use strict';
    const C=root.ChitonConsole=root.ChitonConsole||{};
    const CAMERA_KEY_LIMIT=64,CAMERA_TIME_LIMIT=3600;
    function finite(value,name){if(!Number.isFinite(value))throw new Error('Invalid '+name);return value;}
    function cameraTime(value){finite(value,'keyframe seconds');if(value<0||value>CAMERA_TIME_LIMIT)throw new Error('Time must be 0–3600 seconds');return value;}
    function validateCameraPose(pose){
        if(!pose||!Array.isArray(pose.target)||pose.target.length!==3)throw new Error('Invalid camera target');
        const target=pose.target.map(v=>finite(v,'camera target'));
        const radius=finite(pose.radius,'camera radius'),theta=finite(pose.theta,'camera rotation'),phi=finite(pose.phi,'camera elevation');
        if(target.some(v=>Math.abs(v)>1e9)||radius<.0001||radius>1e9||Math.abs(theta)>1e9||phi<.0001||phi>Math.PI-.0001)throw new Error('Camera pose outside supported bounds');
        return {target,radius,theta,phi};
    }
    class CameraTimeline {
        constructor(){this._keys=[];this._next=1;this.time=0;this.playing=false;this.easing='smooth';this.loop=false;this.projection=null;this._origin=0;this._started=0;this._lastClock=0;}
        get count(){return this._keys.length;}
        get duration(){return this._keys.at(-1)?.time??0;}
        get keyframes(){return this._keys.map(k=>({...k,pose:validateCameraPose(k.pose)}));}
        _ordered(keys){
            keys.sort((a,b)=>a.time-b.time);
            for(let i=1;i<keys.length;i++)if(keys[i].time===keys[i-1].time)throw new Error('Keyframe times must be distinct');
            return keys;
        }
        _commit(keys,now){this.pause(now);this._keys=this._ordered(keys);this.time=Math.min(this.time,this.duration);}
        add(time,pose,projection,now){
            cameraTime(time);const copy=validateCameraPose(pose);
            if(!['perspective','orthographic'].includes(projection))throw new Error('Invalid camera projection');
            if(this.count&&projection!==this.projection)throw new Error('Use the path’s '+this.projection+' projection, or clear the path first');
            if(this.count>=CAMERA_KEY_LIMIT)throw new Error('Maximum 64 camera keyframes');
            if(!Number.isSafeInteger(this._next))throw new Error('Too many camera edits; reload the page');
            const id=this._next,keys=this._ordered([...this._keys,{id,time,pose:copy}]);
            this._commit(keys,now);this._next++;this.projection=projection;return id;
        }
        edit(id,time,pose,now){
            cameraTime(time);const found=this._keys.find(k=>k.id===id);if(!found)throw new Error('Unknown camera keyframe');
            const replacement={...found,time,pose:pose===undefined?found.pose:validateCameraPose(pose)};
            const keys=this._ordered(this._keys.map(k=>k.id===id?replacement:k));this._commit(keys,now);
        }
        remove(id,now){
            if(!this._keys.some(k=>k.id===id))throw new Error('Unknown camera keyframe');
            this._commit(this._keys.filter(k=>k.id!==id),now);if(!this.count)this.projection=null;
        }
        clear(now){this.pause(now);this._keys=[];this.time=0;this.projection=null;}
        options({easing=this.easing,loop=this.loop}={},now){
            if(!['linear','smooth','smoother'].includes(easing)||typeof loop!=='boolean')throw new Error('Invalid camera easing/loop');
            this.pause(now);this.easing=easing;this.loop=loop;
        }
        sample(time=this.time){
            cameraTime(time);if(!this.count)return null;
            const first=this._keys[0],last=this._keys.at(-1);
            if(time<=first.time)return validateCameraPose(first.pose);
            if(time>=last.time)return validateCameraPose(last.pose);
            let n=1;while(this._keys[n].time<time)n++;
            const a=this._keys[n-1],b=this._keys[n];
            let t=(time-a.time)/(b.time-a.time);
            if(this.easing==='smooth')t=t*t*(3-2*t);
            if(this.easing==='smoother')t=t*t*t*(t*(t*6-15)+10);
            const mix=(x,y)=>x+(y-x)*t;
            // Nearest angular arc. Record intermediate views for a full revolution.
            const delta=Math.atan2(Math.sin(b.pose.theta-a.pose.theta),Math.cos(b.pose.theta-a.pose.theta));
            return {target:a.pose.target.map((v,i)=>mix(v,b.pose.target[i])),
                radius:Math.exp(mix(Math.log(a.pose.radius),Math.log(b.pose.radius))),
                theta:a.pose.theta+delta*t,phi:mix(a.pose.phi,b.pose.phi)};
        }
        seek(time){this.time=Math.min(cameraTime(time),this.duration);this.playing=false;return this.sample();}
        play(now){
            finite(now,'camera clock');if(this.count<2||!this.duration)throw new Error('Add at least two distinct-time camera keyframes');
            if(this.playing)return this.advance(now);
            if(this.time>=this.duration)this.time=0;
            this._origin=this.time;this._started=now;this._lastClock=now;this.playing=true;return this.sample();
        }
        advance(now){
            finite(now,'camera clock');
            if(this.playing){
                this._lastClock=Math.max(now,this._lastClock);
                const elapsed=this._origin+Math.max(0,this._lastClock-this._started)/1000;
                if(this.loop)this.time=elapsed%this.duration;
                else {this.time=Math.min(elapsed,this.duration);if(elapsed>=this.duration)this.playing=false;}
            }
            return this.sample();
        }
        pause(now){if(this.playing&&now!==undefined)this.advance(now);this.playing=false;return this.sample();}
        snapshot(){return {time:this.time,duration:this.duration,count:this.count,playing:this.playing,easing:this.easing,loop:this.loop,projection:this.projection};}
    }
    Object.assign(C,{CAMERA_KEY_LIMIT,CAMERA_TIME_LIMIT,validateCameraPose,CameraTimeline});
    if(typeof module==='object')module.exports=C;
})(typeof window==='object'?window:globalThis);
