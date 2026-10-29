/* Single-flight start-to-start reads. No mutation retries or catch-up queues. */
(function(root){
    'use strict';
    const C=root.ChitonConsole=root.ChitonConsole||{};
    class Scheduler {
        constructor({read,onValue=()=>{},onError=()=>{},hz=20,deadline=2000,clock=()=>performance.now(),setTimer=(fn,ms)=>setTimeout(fn,ms),clearTimer=id=>clearTimeout(id)}) {
            Object.assign(this,{read,onValue,onError,hz,deadline,clock,setTimer,clearTimer});
            this.generation=0; this.running=false; this.active=null; this.timer=null; this.failures=0; this.requests=0;
        }
        start() { if(this.running)return; this.running=true; this.generation++; this.queue(0); }
        stop() { this.running=false; this.generation++; this.clearTimer(this.timer); this.timer=null; this.active?.abort(); }
        restart(hz=this.hz) { if(!Number.isFinite(hz)||hz<=0||hz>30)throw new Error('Invalid rate'); this.hz=hz; this.stop(); this.failures=0; this.start(); }
        queue(ms) { this.clearTimer(this.timer); this.timer=this.setTimer(()=>{this.timer=null;this.tick();},ms); }
        async tick() {
            if(!this.running || this.active)return;
            const generation=this.generation, controller=new AbortController(), start=this.clock();
            this.active=controller; this.requests++;
            const timeout=this.setTimer(()=>controller.abort(new Error('Read deadline exceeded')),this.deadline);
            let delay=0;
            try {
                const value=await this.read(controller.signal);
                if(controller.signal.aborted)throw new Error('Read deadline exceeded');
                if(generation===this.generation && this.running) {
                    this.failures=0; this.onValue(value,{latency:this.clock()-start});
                    delay=Math.max(0,1000/this.hz-(this.clock()-start));
                }
            } catch(error) {
                if(generation===this.generation && this.running) {
                    this.failures++; delay=Math.min(8000,500*2**Math.min(4,this.failures-1)); this.onError(error);
                }
            } finally {
                this.clearTimer(timeout); if(this.active===controller)this.active=null;
                if(this.running)this.queue(generation===this.generation?delay:0);
            }
        }
    }
    // Includes headers, body, and parser. Fetch and source operations must honor abort.
    async function bounded(operation,signal,ms=2000) {
        const controller=new AbortController(), abort=()=>controller.abort(signal.reason);
        if(signal?.aborted)controller.abort(signal.reason); else signal?.addEventListener('abort',abort,{once:true});
        const timer=setTimeout(()=>controller.abort(new Error('Request timed out; outcome may be unknown')),ms);
        try { const value=await operation(controller.signal); if(controller.signal.aborted)throw controller.signal.reason; return value; }
        finally { clearTimeout(timer); signal?.removeEventListener('abort',abort); }
    }
    Object.assign(C,{Scheduler,bounded}); if(typeof module==='object')module.exports={Scheduler,bounded};
})(typeof window==='object'?window:globalThis);
