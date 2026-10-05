// Deterministic tests of the actual shipped live-read functions (no browser/dependencies).
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

function deferred() {
    let resolve, reject;
    const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
    return { promise, resolve, reject };
}

async function flush() { for (let i = 0; i < 30; i++) await Promise.resolve(); }

function harness(path) {
    const html = fs.readFileSync(path, 'utf8');
    // Parse complete scripts as well as executing the relevant unmodified functions.
    for (const match of html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/g))
        new vm.Script(match[1], { filename: path });
    assert(html.includes('id="stream_status"'));
    assert(!html.includes('<img src="/image/camera"'));
    const section = (from, to) => {
        const begin = html.indexOf(from), end = html.indexOf(to, begin);
        assert(begin >= 0 && end > begin, from);
        return html.slice(begin, end);
    };
    const script = section('        const STREAM_TIMEOUT', '        function init_point_cloud(') +
        section('        function canvas_down(', '        // Live reads only:') +
        section('        function ratx(', '        function load_snr(') +
        section('        function read_json_response(', '        function apply_frame_type(') +
        section('        function colormode_change2(', '        function upgrade(') +
        section('        function draw_image(', '        GBI("download_png").addEventListener') +
        section('        // Releasing outside the canvas', '        resume_streams();\n\n        //draw_spark();');
    let now = 100000, sequence = 0;
    const timers = new Map(), listeners = {}, requests = [], images = [];
    const urls = new Set(), revoked = [], controls = {};
    const routes = new Map(), active = new Map(), maximum = new Map();
    let draws = 0, renderFailure = false, animationFrames = 0, serverMode = 4;
    const cloud = new Int16Array([0, 0, 0, 0, 20, 30, 40, 1200]).buffer;
    const depth = { value: 1000, mean: 1000, stdd: 2000, stddev: 2, diff: 10, radians: 100,
        frame_id: 1, sample_count: 1, configuration_revision: 1, units: 'depth_sensor_units',
        statistics_kind: 'temporal_observed_frames', frame_type: 'depth', history_reset: false };
    function control(name) {
        if (!controls[name]) controls[name] = {
            value: '', textContent: '', innerText: '', disabled: false,
            width: 640, clientWidth: 640, dataHeight: 480,
            classList: { add() {}, remove() {} },
            setAttribute(name, value) { if (name === 'data-height') this.dataHeight = value; },
            getAttribute() { return this.dataHeight; },
        };
        return controls[name];
    }
    control('colormode').value = control('colormode2').value = '4';
    const window = {
        CHITON_READY: true, CHITON_INTERVAL: 90, CHITON_MODE: 4, CHITON_CLIENT_ID: 'test-ui',
        CHITON_WIDTH: 640, CHITON_HEIGHT: 480, CHITON_SAMP: 0,
        CHITON_WINDOW: [], CHITON_WINDOW_SZ: 100, CHITON_AVG: false,
        CHITONX1: 10, CHITONY1: 20, CHITONX2: 0, CHITONY2: 0,
        CHITON_CENTER_X: 0, CHITON_CENTER_Y: 0,
        CHITON_POS: [], CHITON_COLOR: [], FPS_CTR: 0,
        CHITON_GEOM: { setAttribute() { if (renderFailure) throw new Error('WebGL failed'); } },
        addEventListener: (name, callback) => { listeners[name] = callback; },
    };
    const document = {
        hidden: false,
        addEventListener: (name, callback) => { listeners[name] = callback; },
    };
    function fetch(url, options) {
        assert(options.signal, 'Every live request must be abortable');
        assert.equal(options.cache, 'no-store');
        const route = url.startsWith('/get/value/') ? 'depth' :
            url === '/colormode/setting' ? 'mode-read' :
            url.startsWith('/colormode/') ? 'mode' : url.includes('pointcloud') ? 'cloud' :
            url.includes('image/camera') ? 'image' : 'temp';
        const behavior = routes.get(route) || {};
        const pending = deferred();
        const entry = { url, route, signal: options.signal, pending, behavior };
        requests.push(entry);
        active.set(route, (active.get(route) || 0) + 1);
        maximum.set(route, Math.max(maximum.get(route) || 0, active.get(route)));
        const aborted = () => pending.reject(new Error('Aborted'));
        options.signal.addEventListener('abort', aborted, { once: true });
        if (route === 'mode' && !behavior.hang && !behavior.network && !behavior.badJSON && !behavior.data && !(behavior.status >= 400))
            serverMode = Number(url.split('/').pop());
        const response = {
            headers: { get(name) { return ({ 'X-Chiton-Frame-Id': String(requests.length),
                'X-Chiton-Frame-Age-Ms': '25', 'X-Chiton-Color-Mode': String(behavior.headerMode ?? serverMode) })[name] ?? null; } },
            ok: behavior.status ? behavior.status < 400 : true, status: behavior.status || 200,
            json: () => behavior.body ? behavior.body.promise : behavior.badJSON ?
                Promise.reject(new Error('Invalid JSON')) : Promise.resolve(behavior.data ||
                    (route === 'depth' ? { ...depth, frame_id: requests.length } : route === 'mode-read' ?
                        { value: serverMode, capture_enabled: true } : route === 'mode' ?
                        { value: Number(url.split('/').pop()) } : { cputemp: 42, vixeltemp: 45 })),
            arrayBuffer: () => behavior.body ? behavior.body.promise : Promise.resolve(behavior.data || cloud),
            blob: () => behavior.body ? behavior.body.promise : Promise.resolve({ size: behavior.empty ? 0 : 100 }),
        };
        if (!behavior.hang) {
            if (behavior.network) pending.reject(new Error('Network lost'));
            else pending.resolve(response);
        }
        return pending.promise.finally(() => {
            active.set(route, active.get(route) - 1);
            options.signal.removeEventListener('abort', aborted);
        });
    }
    class Image {
        constructor() { images.push(this); this.naturalWidth = 640; this.naturalHeight = 480; }
        set src(value) {
            this.source = value;
            if (!value) return;
            const behavior = routes.get('image') || {};
            if (!behavior.hangDecode) queueMicrotask(() => {
                if (behavior.badImage) { if (this.onerror) this.onerror(); }
                else if (this.onload) this.onload();
            });
        }
    }
    const context = vm.createContext({
        window, document, AbortController, Image,
        Date: class extends Date { static now() { return now; } },
        URL: {
            createObjectURL() { const url = 'blob:' + ++sequence; urls.add(url); return url; },
            revokeObjectURL(url) { urls.delete(url); revoked.push(url); },
        },
        console: { log() {} },
        setTimeout: (callback, delay) => { const id = ++sequence; timers.set(id, { callback, due: now + delay }); return id; },
        clearTimeout: id => timers.delete(id),
        requestAnimationFrame() { animationFrames++; }, fetch, GBI: control,
        add_info() {}, ratx: x => x, raty: y => y,
        getPosition: () => ({ x: 0, y: 0 }),
        init_point_cloud() {}, draw_calibration_line() {}, chiton_line() {},
        canvas_card: { clientWidth: 664 }, canvas_elm: control('canvas'),
        ctx: { drawImage() { if (renderFailure) throw new Error('Canvas failed'); draws++; } },
        THREE: {
            Color: class { setRGB(r, g, b) { Object.assign(this, { r, g, b }); } },
            Float32BufferAttribute: class { constructor(array) { this.array = array; } },
        },
    });
    vm.runInContext(script, context, { filename: path });
    async function advance(milliseconds) {
        const target = now + milliseconds;
        await flush();
        let count = 0;
        while (true) {
            const next = [...timers.entries()].filter(([, timer]) => timer.due <= target)
                .sort((a, b) => a[1].due - b[1].due)[0];
            if (!next) break;
            assert(++count < 1000, 'Runaway polling');
            now = next[1].due; timers.delete(next[0]); next[1].callback();
            await flush();
        }
        now = target; await flush();
    }
    return { context, window, document, routes, requests, timers, listeners, controls,
        images, urls, revoked, maximum, advance,
        get draws() { return draws; }, set serverMode(value) { serverMode = value; }, set renderFailure(value) { renderFailure = value; },
        get animationFrames() { return animationFrames; },
        state: name => context.stream_state(name),
        count: route => requests.filter(request => request.route === route).length,
    };
}

async function testStreams(path) {
    // Failed status, truncated payload, rendering exception and restart all release the gate.
    const h = harness(path), c = h.context;
    h.routes.set('cloud', { status: 503 });
    await c.load_frame();
    assert.equal(h.window.CHITON_READY, true);
    assert.equal(h.state('frame').failures, 1);
    assert.match(h.controls.stream_status.textContent, /disconnected\/stale.*HTTP 503/);
    c.load_frame(); assert.equal(h.count('cloud'), 1); // Backoff prevents immediate hammering.
    for (const bytes of [0, 2, 10, 640 * 480 * 8 + 8]) {
        await h.advance(8000);
        h.routes.set('cloud', { data: new ArrayBuffer(bytes) });
        await c.load_frame();
        assert.match(h.state('frame').error, /Invalid\/truncated/);
        assert.equal(h.window.CHITON_READY, true);
    }
    h.routes.delete('cloud'); await h.advance(8000);
    h.renderFailure = true; await c.load_frame();
    assert.match(h.state('frame').error, /WebGL failed/);
    h.renderFailure = false; await h.advance(8000); await c.load_frame();
    assert.equal(h.state('frame').failures, 0);
    assert.equal(h.state('frame').phase, 'live');
    assert.deepEqual(Array.from(h.window.CHITON_POS), [-20, 30, 40]);
    h.routes.set('cloud', { data: new Int16Array([60, 70, 80, 1200]).buffer });
    await h.advance(90); await c.load_frame();
    assert.deepEqual(Array.from(h.window.CHITON_POS), [-60, 70, 80]); // First record is not metadata.
    h.routes.delete('cloud');
    await h.advance(6000); c.render_stream_status();
    assert.match(h.controls.stream_status.textContent, /frame: stale.*last update 6s ago/);

    // Fetch and body deadlines, single-flight, capped backoff, then successful recovery.
    for (const behavior of [{ hang: true }, { body: deferred() }]) {
        c.cancel_stream('frame'); h.routes.set('cloud', behavior);
        const pending = c.load_frame(); await flush();
        assert.equal(h.window.CHITON_READY, false);
        const count = h.count('cloud');
        assert.equal(c.load_frame(), pending);
        await h.advance(9999); assert.equal(h.count('cloud'), count);
        assert.equal(h.window.CHITON_READY, false);
        await h.advance(1); await pending;
        assert.equal(h.requests.at(-1).signal.aborted, true);
        assert.equal(h.window.CHITON_READY, true);
        assert.match(h.state('frame').error, /timed out/);
        h.routes.delete('cloud'); await h.advance(500); await c.load_frame();
        assert.equal(h.state('frame').phase, 'live');
        if (behavior.body) { behavior.body.resolve(new ArrayBuffer(16)); await flush(); }
    }
    h.routes.set('cloud', { network: true }); c.cancel_stream('frame');
    for (let failure = 1; failure <= 7; failure++) {
        await c.load_frame();
        const delay = Math.min(8000, 500 * 2 ** (failure - 1));
        await h.advance(delay - 1);
        const count = h.count('cloud'); c.load_frame(); assert.equal(h.count('cloud'), count);
        await h.advance(1);
    }
    assert.equal(h.maximum.get('cloud'), 1);
    h.window.CHITON_CAM = {};
    h.window.CHITON_REND = { render() { throw new Error('Renderer failed'); } };
    h.context.points = { rotation: {} };
    assert.throws(() => c.load_data(0), /Renderer failed/);
    assert.equal(h.animationFrames, 1); // Next frame is queued even if rendering throws.
    await flush();

    // Depth/temperature schedule only after body completion; obsolete selection is discarded.
    const p = harness(path), pc = p.context;
    const oldBody = deferred(); p.routes.set('depth', { body: oldBody });
    const depthTask = pc.load_depth(); pc.load_depth(); await flush();
    assert.equal(p.count('depth'), 1); assert.equal(p.state('depth').timer, null);
    await p.advance(1000); assert.equal(p.count('depth'), 1);
    pc.canvas_down({ clientX: 30, clientY: 40 });
    await depthTask; await p.advance(0);
    assert.equal(p.count('depth'), 1); // No requests while selecting.
    p.routes.delete('depth'); p.listeners.mouseup(); await p.advance(0);
    assert.equal(p.requests.findLast(request => request.route === 'depth').url, '/get/value/client/test-ui/30/40');
    const history = p.window.CHITON_WINDOW.length;
    oldBody.resolve({ value: 9999, mean: 9999, stdd: 0, diff: 0 }); await flush();
    assert.equal(p.window.CHITON_WINDOW.length, history);
    assert.equal(p.window.CHITON_SAMP, 1000);
    pc.cancel_stream('temp'); await flush();
    const tempCount = p.count('temp');
    const tempBody = deferred(); p.routes.set('temp', { body: tempBody });
    const tempTask = pc.load_temp(); pc.load_temp(); await p.advance(5000);
    assert.equal(p.count('temp'), tempCount + 1); assert.equal(p.state('temp').timer, null);
    tempBody.resolve({ cputemp: 41, vixeltemp: 43 }); await tempTask;
    await p.advance(3999); assert.equal(p.count('temp'), tempCount + 1);
    p.routes.delete('temp'); await p.advance(1); assert.equal(p.count('temp'), tempCount + 2);
    assert.equal(p.controls.cputemp.textContent, '42.0°');
    assert.equal(p.maximum.get('depth'), 1); assert.equal(p.maximum.get('temp'), 1);
    // Normal canvas mouseup (not just releasing outside) must restart the polling timer.
    const beforeSelection = p.count('depth');
    pc.canvas_down({ clientX: 50, clientY: 60 });
    pc.canvas_up({ clientX: 51, clientY: 61 }); await flush();
    assert.equal(p.count('depth'), beforeSelection + 1);
    assert.equal(p.requests.findLast(request => request.route === 'depth').url, '/get/value/client/test-ui/50/60');
    await p.advance(90); assert.equal(p.count('depth'), beforeSelection + 2);
    // Cloud mode hides the canvas (clientWidth == 0); its last pixel selection stays valid.
    p.controls.canvas.clientWidth = 0;
    await p.advance(90); assert.equal(p.count('depth'), beforeSelection + 3);
    assert.equal(p.requests.findLast(request => request.route === 'depth').url, '/get/value/client/test-ui/50/60');

    // HTTP/malformed JSON/invalid numbers leave acknowledged values intact and recover.
    for (const behavior of [{ status: 500 }, { badJSON: true }, { data: { value: 'bad' } }]) {
        pc.cancel_stream('depth'); p.routes.set('depth', behavior);
        await pc.load_depth(); assert.equal(p.window.CHITON_SAMP, 1000);
        assert.equal(p.state('depth').failures, 1);
        p.routes.delete('depth'); await p.advance(500);
        assert.equal(p.state('depth').phase, 'live');
    }
    pc.cancel_stream('temp'); p.routes.set('temp', { data: { cputemp: null, vixeltemp: 2 } });
    await pc.load_temp(); assert.equal(p.controls.cputemp.textContent, '42.0°');
    assert.match(p.state('temp').error, /Invalid measurement/);

    // Hidden/page-away views abort and stop polling, then resume without overlapping requests.
    pc.cancel_stream('frame'); p.routes.set('cloud', { hang: true });
    const hiddenTask = pc.load_frame(); await flush();
    pc.canvas_down({ clientX: 50, clientY: 60 });
    p.document.hidden = true; p.listeners.visibilitychange(); await hiddenTask;
    assert.equal(p.window.CHITON_SELECTING, false); // Hiding mid-drag must not stall resumption.
    const hiddenCount = p.requests.length;
    await p.advance(30000); pc.load_frame(); pc.load_depth(); pc.load_temp();
    assert.equal(p.requests.length, hiddenCount);
    assert.match(p.controls.stream_status.textContent, /paused/);
    p.routes.clear(); p.document.hidden = false; p.listeners.visibilitychange();
    await pc.load_frame(); await flush();
    assert.equal(p.state('frame').phase, 'live'); assert.equal(p.state('depth').phase, 'live');
    p.listeners.pagehide(); await flush(); const away = p.requests.length;
    await p.advance(30000); pc.load_frame(); assert.equal(p.requests.length, away);
    p.listeners.pageshow(); await flush(); assert(p.requests.length > away);

    // JPEG decoding errors, empty responses, decode deadlines and cancellation revoke every URL.
    const i = harness(path), ic = i.context;
    i.controls.colormode.value = '1'; i.window.CHITON_MODE = 1; i.serverMode = 1;
    for (const behavior of [{ badImage: true }, { empty: true }, { hangDecode: true }]) {
        ic.cancel_stream('frame'); i.routes.set('image', behavior);
        const imageTask = ic.load_frame(); await flush();
        if (behavior.hangDecode) await i.advance(10000);
        await imageTask;
        assert.equal(i.window.CHITON_READY, true); assert.equal(i.urls.size, 0);
        assert.equal(i.draws, 0); assert.equal(i.state('frame').failures, 1);
    }
    i.routes.delete('image'); await i.advance(8000); await ic.load_frame();
    assert.equal(i.draws, 1); assert.equal(i.urls.size, 0);
    const fps = i.window.FPS_CTR;
    i.renderFailure = true; await i.advance(90); await ic.load_frame();
    assert.equal(i.window.CHITON_READY, true); assert.match(i.state('frame').error, /Canvas failed/);
    assert.equal(i.window.FPS_CTR, fps);
    i.renderFailure = false; ic.cancel_stream('frame'); i.routes.set('image', { hangDecode: true });
    const obsoleteImage = ic.load_frame(); await flush();
    i.controls.colormode.value = '4'; await ic.colormode_change(); await obsoleteImage;
    i.routes.delete('image'); await ic.load_frame();
    assert.equal(i.urls.size, 0); assert.equal(i.state('frame').phase, 'live');
    assert.equal(i.window.CHITON_MODE, 4);

    // A stalled mode mutation must not freeze the viewer or get automatically retried.
    i.controls.colormode.value = '1'; i.routes.set('mode', { hang: true });
    const changing = ic.colormode_change(); await flush();
    assert.equal(i.controls.colormode.disabled, true); assert.equal(ic.load_frame(), undefined);
    const mutations = i.count('mode');
    await i.advance(10000); await changing;
    assert.equal(i.count('mode'), mutations); assert.equal(i.window.CHITON_MODE_PENDING, false);
    assert.equal(i.controls.colormode.disabled, false); assert.equal(i.controls.colormode.value, 4);
    await ic.load_frame(); assert.equal(i.state('frame').phase, 'live');
    await i.advance(16000); assert.equal(i.count('mode'), mutations);

    // F20: every supported mode transitions through JPEG/cloud/planar states;
    // every mutation failure rolls back once, then subsequent requests recover.
    const modes = harness(path), modesContext = modes.context;
    for (const mode of [0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0]) {
        modes.controls.colormode.value = String(mode);
        await modesContext.colormode_change(); await modesContext.load_frame(); await flush();
        assert.equal(modes.window.CHITON_MODE, mode);
        assert.equal(modes.window.CHITON_WIDTH, mode > 4 ? 1500 : 640);
        assert.equal(modes.window.CHITON_HEIGHT, mode > 4 ? 165 : 480);
        assert.equal(modes.state('frame').phase, 'live');
        assert.equal(modes.requests.findLast(entry => ['cloud', 'image'].includes(entry.route)).route,
            mode === 4 ? 'cloud' : 'image');
        for (const behavior of [{ status: 500 }, { network: true }, { badJSON: true }, { data: { value: -1 } }]) {
            modes.routes.set('mode', behavior);
            modes.controls.colormode.value = String((mode + 1) % 7);
            const attempts = modes.count('mode');
            await modesContext.colormode_change(); await modesContext.load_frame(); await flush();
            assert.equal(modes.count('mode'), attempts + 1);
            assert.equal(modes.window.CHITON_MODE, mode);
            assert.equal(Number(modes.controls.colormode.value), mode);
            assert.equal(modes.controls.colormode.disabled, false);
            assert.equal(modes.window.CHITON_MODE_PENDING, false);
            assert.equal(modes.state('frame').phase, 'live');
            await modes.advance(1000);
            assert.equal(modes.count('mode'), attempts + 1); // Mutations never retry.
            modes.routes.delete('mode');
        }
    }

    // F09: reject a payload/header format mismatch without drawing it.
    const invalidMode = harness(path), invalidContext = invalidMode.context;
    invalidMode.routes.set('cloud', { headerMode: 1 });
    await invalidContext.load_frame();
    assert.match(invalidMode.state('frame').error, /Cloud response has incompatible mode/);
    assert.equal(invalidMode.window.CHITON_READY, true);
    invalidMode.serverMode = 1;
    invalidMode.routes.delete('cloud');
    invalidMode.routes.set('image', { headerMode: 4 });
    await invalidMode.advance(500); await invalidContext.load_frame();
    assert.match(invalidMode.state('frame').error, /JPEG response has incompatible mode/);
    assert.equal(invalidMode.draws, 0);

    // F09: another viewer changes the device, never this tab's retrieval.
    const shared = harness(path), sc = shared.context;
    await sc.load_frame();
    assert.match(shared.controls.stream_status.textContent, /host receipt age 0s.*not exposure time/);
    shared.serverMode = 5;
    await shared.advance(90); await sc.load_frame();
    assert.equal(shared.window.CHITON_MODE, 5);
    assert.equal(shared.controls.colormode.value, 5);
    assert.equal(shared.controls.colormode2.value, 5);
    assert.equal(shared.window.CHITON_WIDTH, 1500);
    assert.equal(shared.count('mode'), 0, 'Observational reads sent a mode mutation');
    assert(shared.requests.findLast(r => r.route === 'image').url.includes('?after='));
    shared.serverMode = 4;
    await shared.advance(90); await sc.load_frame();
    assert.equal(shared.window.CHITON_MODE, 4);
    assert.equal(shared.count('mode'), 0);
    shared.routes.set('mode-read', { data: { value: 4, capture_enabled: true, frame_id: 0 } });
    await shared.advance(90); await sc.load_frame();
    assert(!shared.requests.findLast(r => r.route === 'cloud').url.includes('?after='),
        'Restarted frame sequence retained the old process ID');
    shared.routes.set('mode-read', { data: { value: 2, capture_enabled: false } });
    await shared.advance(90); await sc.load_frame();
    assert.equal(shared.window.CHITON_MODE, 2);
    assert.match(shared.state('frame').error, /Capture disabled/);
    const downloads = shared.count('cloud') + shared.count('image');
    await shared.advance(1000); await sc.load_frame();
    assert.equal(shared.count('cloud') + shared.count('image'), downloads);
    assert.match(shared.controls.stream_status.textContent, /host receipt age [1-9]/);

    // F11: real acknowledged values, exact means/deviations, and no chart duplicates.
    const m = harness(path), mc = m.context;
    const sample = { value: 1234, mean: 1234.5, stdd: 500, stddev: 0.5, diff: 1,
        frame_id: 11, sample_count: 2, configuration_revision: 3,
        units: 'depth_sensor_units', statistics_kind: 'temporal_observed_frames', history_reset: false };
    m.window.CHITON_AVG = true; m.window.CHITON_SAMP = 9000;
    m.routes.set('depth', { data: sample }); await mc.load_depth();
    assert.equal(m.window.CHITON_SAMP, 1234); // No implicit browser blend.
    assert.equal(m.controls.mean.innerText, '1234.500');
    assert.equal(m.controls.stdd.innerText, '0.500');
    assert.equal(m.window.CHITON_WINDOW.length, 1);
    await m.advance(90); assert.equal(m.window.CHITON_WINDOW.length, 1); // Same frame.
    sample.frame_id = 12; sample.value = 1235;
    await m.advance(90); assert.equal(m.window.CHITON_WINDOW.length, 2);
    sample.configuration_revision = 4; sample.units = 'ir_intensity_units';
    await m.advance(90); assert.equal(m.window.CHITON_WINDOW.length, 1);
    assert.match(m.controls.measurement_context.textContent, /ir_intensity_units.*observed frames: 2/);
    sample.frame_id = 13; await m.advance(90); assert.equal(m.window.CHITON_WINDOW.length, 2);
    sample.history_reset = true; await m.advance(90); assert.equal(m.window.CHITON_WINDOW.length, 1);
    sample.frame_id = NaN; await m.advance(90);
    assert.equal(m.window.CHITON_SAMP, 1235); assert.equal(m.state('depth').failures, 1);
}

module.exports = testStreams;
if (require.main === module) (async () => {
    for (const path of ['release/files/app.html', 'bin/files/app.html']) await testStreams(path);
    console.log('Web stream recovery tests passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
