'use strict';
const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
async function check(path) {
    let resolve, requests = 0, results = 0, signal;
    const window = {fetch: (_, options) => {
        requests++; signal = options.signal;
        return new Promise(r => { resolve = r; });
    }};
    vm.runInNewContext(fs.readFileSync(path, 'utf8'), {window, AbortController, setTimeout, clearTimeout, Date});
    const reader = new window.ChitonMetricsReader(() => results++);
    const pending = reader.poll(false);
    await reader.poll(false); assert.equal(requests, 1);
    reader.stop(); assert(signal.aborted);
    resolve({ok:true, json:async () => ({pipeline:{system:{}}})}); await pending;
    assert.equal(results, 0); assert.equal(reader.controller, null);
    const fresh = reader.poll(false);
    resolve({ok:true, json:async () => ({clients:2, pipeline:{system:{process_cpu_percent:12}}})}); await fresh;
    assert.equal(results, 1); assert(reader.updatedAt > 0);
    await reader.poll(true); assert.equal(requests, 2);
    const text = window.chitonFormatDiagnostics({clients:2, pipeline:{system:{process_cpu_percent:null,
        camera_cpu_percent:5, thermal_zones:[], per_core:[], cooling_devices:[], throttled:null}}});
    assert(text.includes('unavailable/5.00%')); assert(text.includes('throttle unknown')); assert(text.includes('clients 2'));
}
(async () => {
    await check('bin/files/image-stream.js'); await check('release/files/image-stream.js');
    console.log('performance browser tests passed');
})().catch(e => { console.error(e); process.exitCode = 1; });
