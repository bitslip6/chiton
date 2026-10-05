// Dependency-free tests of the deliberately reduced diagnostic page.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
async function flush() { for (let i = 0; i < 30; ++i) await Promise.resolve(); }
async function test(path) {
    const html = fs.readFileSync(path, 'utf8');
    assert(!html.includes('/get/value/'));
    const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
    const controls = Object.fromEntries(['image','frame','mode','greyscale','poll','status','preview'].map(id => [id, {checked:false, disabled:false, hidden:true, textContent:''}]));
    const requests = [], timers = new Map(), listeners = {}, urls = new Set();
    let now = 0, seq = 0, mode = 'ok', held = null, responseStatus = 200, invalidDecode = false;
    const document = {hidden:false, getElementById:id => controls[id],
        querySelectorAll:() => ['image','frame','mode','greyscale'].map(id => controls[id]),
        addEventListener:(name, fn) => listeners[name] = fn};
    const context = vm.createContext({document, window:{addEventListener:(name, fn) => listeners[name] = fn},
        AbortController, Blob, TextDecoder,
        Date:class extends Date { static now() { return now; } },
        URL:{createObjectURL() { const url = 'blob:' + ++seq; urls.add(url); return url; }, revokeObjectURL:url => urls.delete(url)},
        Image:class { constructor() { this.naturalWidth=640; this.naturalHeight=480; }
            set src(value) { if (value) queueMicrotask(() => invalidDecode ? this.onerror?.() : this.onload?.()); } },
        setTimeout:(callback, delay) => { const id = ++seq; timers.set(id,{callback,due:now+delay}); return id; },
        clearTimeout:id => timers.delete(id),
        fetch:(url, options) => {
            assert.equal(options.cache,'no-store'); assert(options.signal);
            requests.push(url);
            const response = {status:responseStatus, ok:responseStatus < 400,
                headers:{get:name => ({'Content-Type':responseStatus === 200 ? 'image/jpeg' : 'application/json', 'Content-Length':'4', 'X-Chiton-Frame-Id':'7'})[name] ?? null},
                arrayBuffer:async () => responseStatus === 200 ? new Uint8Array(mode === 'bad' ? [1,2,3,4] : [255,216,255,217]).buffer : new TextEncoder().encode('{"error":"JPEG unavailable in cloud mode"}').buffer};
            if (mode !== 'hold') return Promise.resolve(response);
            return new Promise((resolve, reject) => {
                held = () => resolve(response);
                options.signal.addEventListener('abort', () => reject(new Error('Aborted')), {once:true});
            });
        }});
    vm.runInContext(script, context, {filename:path});
    async function advance(ms) {
        now += ms;
        for (const [id,timer] of [...timers]) if (timer.due <= now) {timers.delete(id); timer.callback();}
        await flush();
    }
    assert.equal(requests.length,0); // No automatic reads or mutations.
    await controls.image.onclick();
    assert.deepEqual(requests,['/image/camera']);
    assert.match(controls.status.textContent,/Decoded 640×480/);
    assert.equal(controls.preview.hidden,false); assert.equal(urls.size,1);
    mode='hold';
    const pending=controls.image.onclick(); await flush();
    await controls.frame.onclick(); assert.equal(requests.length,2); // One request, including body/decode, at a time.
    held(); await pending;
    assert.equal(urls.size,1); // Old preview URL released.
    mode='bad'; await controls.image.onclick();
    assert.match(controls.status.textContent,/Missing JPEG start marker/);
    mode='ok'; invalidDecode=true; await controls.image.onclick();
    assert.match(controls.status.textContent,/could not decode JPEG/); assert.equal(urls.size,1);
    invalidDecode=false; responseStatus=409; await controls.image.onclick();
    assert.match(controls.status.textContent,/JPEG unavailable in cloud mode/);
    responseStatus=200;
    controls.poll.checked=true; controls.poll.onchange();
    const count=requests.length;
    await advance(1999); assert.equal(requests.length,count);
    await advance(1); assert.equal(requests.length,count+1);
    await advance(1999); assert.equal(requests.length,count+1);
    await advance(1); assert.equal(requests.length,count+2);
    controls.poll.checked=false; controls.poll.onchange();
    mode='hold'; const timeout=controls.image.onclick(); await advance(10000); await timeout;
    assert.match(controls.status.textContent,/timeout or navigation/);
    mode='ok'; const before=requests.length;
    await controls.greyscale.onclick(); assert.equal(requests.at(-1),'/colormode/setting/1');
    await advance(5000); assert.equal(requests.length,before+1); // No mutation retries.
    listeners.pagehide(); assert.equal(urls.size,0);
    await advance(5000); assert.equal(requests.length,before+1);
    assert(requests.every(url => !url.includes('/get/value/')));
}
module.exports = test;
if (require.main === module) (async () => {
    for (const path of ['release/files/debug.html','bin/files/debug.html']) await test(path);
    assert.equal(fs.readFileSync('release/files/debug.html','utf8'),fs.readFileSync('bin/files/debug.html','utf8'));
    console.log('Image diagnostic UI tests passed');
})().catch(error => {console.error(error);process.exitCode=1;});
