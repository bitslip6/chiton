const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
module.exports = async function test(path) {
    const sockets=[], errors=[], rendered=[], closed=[];
    const timers=new Map(), raf=new Map(); let next=1, delayed=null;
    const root={location:{protocol:'http:',host:'sensor:8080'},
        WebSocket:class { constructor(url,protocol){this.url=url;this.protocol=protocol;this.sent=[];sockets.push(this);} send(data){this.sent.push(data);} close(){this.closed=true;} },
        createImageBitmap:async () => delayed ? await delayed : {width:640,height:480,close(){closed.push(this);}},
        requestAnimationFrame:fn=>{const id=next++;raf.set(id,fn);return id;},
        cancelAnimationFrame:id=>raf.delete(id)};
    const context=vm.createContext({window:root,ArrayBuffer,DataView,Uint8Array,TextDecoder,Blob,AbortController,Date,
        setTimeout:fn=>{const id=next++;timers.set(id,fn);return id;},clearTimeout:id=>timers.delete(id)});
    vm.runInContext(fs.readFileSync(path,'utf8'),context);
    const stream=new root.ChitonImageStream({fps:()=>30,onError:e=>errors.push(e.message),onFrame:(image,meta)=>rendered.push(meta.id),onStatus:()=>{}});
    function frame(id=1, overrides={}) {
        const meta=Buffer.from(JSON.stringify({version:1,frame_id:id,mode:1,width:640,height:480,age_ms:0,...overrides}));
        const packet=Buffer.alloc(4+meta.length+4);packet.writeUInt32BE(meta.length);meta.copy(packet,4);packet.set([255,216,255,217],4+meta.length);
        return packet.buffer.slice(packet.byteOffset,packet.byteOffset+packet.byteLength);
    }
    assert(stream.start());assert(stream.start());assert.equal(sockets.length,1);
    assert.equal(sockets[0].url,'ws://sensor:8080/stream/camera');assert.equal(sockets[0].protocol,'chiton.jpeg.v1');
    await sockets[0].onmessage({data:frame(1)});
    await sockets[0].onmessage({data:frame(2)});
    assert.deepEqual(sockets[0].sent,['1','2']);assert.equal(raf.size,1);
    assert.equal(closed.length,1);assert.equal(stream.stats.dropped,1);
    const paint=raf.values().next().value;raf.clear();paint(100);
    assert.deepEqual(rendered,[2]);assert.equal(closed.length,2);
    await sockets[0].onmessage({data:frame(3)});stream.invalidate();assert.equal(stream.pending,null);
    const pendingPaint=raf.values().next().value;raf.clear();pendingPaint(150);
    assert.deepEqual(rendered,[2]);
    await sockets[0].onmessage({data:frame(4,{mode:4})});
    assert.equal(errors.length,1);assert.equal(stream.socket,null);assert(!stream.start());
    stream.retryAt=0;assert(stream.start());
    let resolve;delayed=new Promise(done=>resolve=done);
    const decoding=sockets[1].onmessage({data:frame(5)});
    assert(stream.busy);stream.stop();assert(!stream.start(), 'Never stack uninterruptible native decoders');
    const obsolete={width:640,height:480,close(){closed.push(this);}};
    resolve(obsolete);await decoding;delayed=null;
    assert(closed.includes(obsolete));assert.equal(stream.pending,null);assert(!stream.busy);
    assert(stream.start());
    const watchdog=timers.values().next().value;watchdog();
    assert.equal(errors.length,2);assert(errors.at(-1).includes('timed out'));
    stream.stop();assert.equal(timers.size,0);assert.equal(raf.size,0);
};
if(require.main===module) (async()=>{for(const path of ['release/files/image-stream.js','bin/files/image-stream.js']) await module.exports(path);console.log('Browser JPEG credit, decoding, latest-only rendering, cancellation and fallback tests passed');})().catch(e=>{console.error(e);process.exitCode=1;});
