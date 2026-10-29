/* Scalar random walk; demonstration tuning is not calibrated sensor accuracy. */
(function(root){
    'use strict';
    const C=root.ChitonConsole=root.ChitonConsole||{};
    const presets={Responsive:{R:25,q:1250},Balanced:{R:25,q:125},Stable:{R:25,q:12.5}};
    class Kalman {
        constructor(parameters=presets.Balanced){this.configure(parameters);}
        configure({q,R}) {
            if(!Number.isFinite(q)||q<0||q>1e9||!Number.isFinite(R)||R<=0||R>1e9)throw new Error('Require 0 ≤ q ≤ 1e9 and 0 < R ≤ 1e9');
            this.q=q; this.R=R; this.reset('Parameters changed');
        }
        reset(reason='New segment'){this.x=null;this.P=null;this.time=null;this.frame=null;this.reason=reason;}
        update(value,time,frame){
            if(!Number.isFinite(value)||value<0||value>65535||!Number.isSafeInteger(frame)||frame<1)throw new Error('Invalid Kalman sample');
            if(this.frame!=null && frame<=this.frame)return this.x;
            if(!Number.isFinite(time)||time<=0){this.reset('Timestamp unavailable');return null;}
            const dt=this.time==null?null:(time-this.time)/1000;
            if(this.x==null || dt<=0 || dt>2){
                this.reason=dt==null?'Initializing':dt<=0?'Timestamp discontinuity':'Gap over 2 s';this.x=value;this.P=this.R;
            }else{
                const predicted=this.P+this.q*dt, gain=predicted/(predicted+this.R);
                this.x+=gain*(value-this.x);this.P=(1-gain)*predicted;this.reason='Tracking';
            }
            this.time=time;this.frame=frame;return this.x;
        }
    }
    Object.assign(C,{Kalman,presets});if(typeof module==='object')module.exports={Kalman,presets};
})(typeof window==='object'?window:globalThis);
