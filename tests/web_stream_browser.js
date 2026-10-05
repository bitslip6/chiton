// Optional real Chromium check: local mock sensor only; all external traffic is blocked.
// Run from the repository root: node tests/web_stream_browser.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { spawn } = require('node:child_process');

async function main() {
    const assets = path.resolve(process.env.CHITON_ASSET_DIR || 'release/files');
    const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'chiton-viewer-browser-'));
    const behavior = { image: 'ok', cloud: 'ok', depth: 'ok', temp: 'ok' };
    const active = {}, maximum = {}, counts = {}, exceptions = [], overlaps = [];
    const downloadEvents = [];
    let mockFrame = 0, exportFailure = false, mockMode = 1;
    const raw = Buffer.alloc(640 * 480 * 2);
    for (let offset = 0; offset < raw.length; offset += 2) raw.writeUInt16LE(800, offset);
    const obj = Buffer.from('v 0.0000 0.0000 0.0000\nv 0.0033 0.0050 0.0067\n');
    const cloud = Buffer.from(new Int16Array([0, 0, 0, 0, 20, 30, 40, 1200]).buffer);
    const image = fs.readFileSync(path.join(assets, 'avatar.jpg'));
    const mime = { '.html': 'text/html', '.js': 'application/javascript', '.css': 'text/css',
        '.png': 'image/png', '.jpg': 'image/jpeg', '.svg': 'image/svg+xml', '.woff2': 'font/woff2' };
    const server = http.createServer((req, res) => {
        const url = new URL(req.url, 'http://' + req.headers.host);
        // Also act as a rejecting proxy for Chromium's background traffic.
        if (url.hostname !== '127.0.0.1' || Number(url.port) !== server.address()?.port) {
            res.writeHead(403); res.end(); return;
        }
        const route = url.pathname.includes('/image/camera') ? 'image' :
            url.pathname.includes('/pointcloud/camera') ? 'cloud' :
            url.pathname.startsWith('/get/value/') ? 'depth' :
            url.pathname === '/temp/setting' ? 'temp' : null;
        const snapshotMode = mockMode;
        function send(type, body, status = 200, headers = {}) {
            if (res.destroyed) return;
            const frameHeaders = route === 'cloud' || route === 'image' ? {
                'X-Chiton-Frame-Id': String(++mockFrame), 'X-Chiton-Frame-Age-Ms': '25',
                'X-Chiton-Color-Mode': String(snapshotMode) } : {};
            res.writeHead(status, { ...frameHeaders, 'Content-Type': type, 'Content-Length': body.length, 'Connection': 'close', ...headers }); res.end(body);
        }
        const json = data => send('application/json', Buffer.from(JSON.stringify(data)));
        if (url.pathname === '/raw/camera' || url.pathname === '/pointcloud_obj/camera') {
            if (exportFailure) return send('application/json', Buffer.from('{"error":"not ready","status":503}'), 503);
            const isRaw = url.pathname === '/raw/camera';
            return send(isRaw ? 'application/octet-stream' : 'text/plain; charset=utf-8', isRaw ? raw : obj, 200,
                {'Content-Disposition': 'attachment; filename="' + (isRaw ? 'chiton-raw.bin' : 'chiton-pointcloud.obj') + '"'});
        }
        if (route) {
            active[route] = (active[route] || 0) + 1;
            maximum[route] = Math.max(maximum[route] || 0, active[route]);
            counts[route] = (counts[route] || 0) + 1;
            res.once('close', () => { active[route]--; });
            const state = behavior[route];
            if (active[route] > 1) overlaps.push({ route, state, active: active[route], count: counts[route] });
            if (state === 'hang') return;
            if (state === 'hangBody') {
                res.writeHead(200, { 'Content-Type': 'application/octet-stream', 'Content-Length': 16, 'Connection': 'close',
                    'X-Chiton-Frame-Id': String(++mockFrame), 'X-Chiton-Frame-Age-Ms': '25',
                    'X-Chiton-Color-Mode': String(snapshotMode) });
                res.write(Buffer.alloc(2)); return;
            }
            setTimeout(() => {
                if (state === 'error') return send('application/json', Buffer.from('{"error":"not ready"}'), 503);
                if (state === 'disconnect') return res.destroy();
                if (route === 'cloud') return send('application/octet-stream', state === 'invalid' ? Buffer.alloc(10) : cloud);
                if (route === 'image') return send('image/jpeg', state === 'invalid' ? Buffer.from('not a JPEG') : image);
                if (route === 'depth') return json({ value: 1000, mean: 1000, stdd: 1000, stddev: 1, diff: 10,
                    frame_id: ++mockFrame, configuration_revision: 1, sample_count: 1,
                    units: 'depth_sensor_units', statistics_kind: 'temporal_observed_frames' });
                json({ cputemp: 42, vixeltemp: 45 });
            }, 150);
            return;
        }
        if (url.pathname === '/colormode/setting') return json({ value: mockMode, capture_enabled: true, frame_id: mockFrame });
        if (url.pathname.startsWith('/colormode/setting/')) {
            counts['mode-mutation'] = (counts['mode-mutation'] || 0) + 1;
            mockMode = Number(url.pathname.split('/').pop()); return json({ value: mockMode });
        }
        if (url.pathname === '/depth/setting') return json({ value: true, frame_type: 'depth' });
        if (url.pathname === '/range/setting') return json({ min: 0, max: 6500 });
        if (url.pathname === '/dimension/camera') return json({ inner_dist: 50, outer_dist: 200, center_x: 320, center_y: 240 });
        if (url.pathname === '/radians/setting') return json(Object.fromEntries(Array.from({ length: 8 }, (_, i) => ['radian' + (i + 1), i])));
        const filename = path.resolve(assets, '.' + (url.pathname === '/' ? '/app.html' : url.pathname));
        if (!filename.startsWith(assets + path.sep)) return send('text/plain', Buffer.from('Not found'), 404);
        fs.readFile(filename, (error, data) => error ? send('text/plain', Buffer.from('Not found'), 404) :
            send(mime[path.extname(filename)] || 'application/octet-stream', data));
    });
    server.on('connect', (req, socket) => socket.destroy());
    let browser, session, closing = false, failure = '';
    const pending = new Map();
    let nextId = 0, buffered = '';
    function command(method, params = {}, sessionId = session) {
        return new Promise((resolve, reject) => {
            const id = ++nextId;
            const timer = setTimeout(() => { pending.delete(id); reject(new Error('CDP timeout: ' + method)); }, 15000);
            pending.set(id, { resolve, reject, timer });
            browser.stdio[3].write(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }) + '\0');
        });
    }
    async function evaluate(expression, userGesture = false) {
        const result = await command('Runtime.evaluate', { expression, userGesture, awaitPromise: true, returnByValue: true });
        if (result.exceptionDetails) throw new Error(result.exceptionDetails.text + ': ' + result.exceptionDetails.exception?.description);
        return result.result.value;
    }
    async function wait(expression, timeout = 6000) {
        const end = Date.now() + timeout;
        while (Date.now() < end) {
            if (await evaluate(expression)) return;
            await new Promise(resolve => setTimeout(resolve, 50));
        }
        throw new Error('Browser condition timed out: ' + expression + '\n' + await evaluate('GBI("stream_status").textContent'));
    }
    try {
        await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
        const port = server.address().port;
        const origin = 'http://127.0.0.1:' + port;
        browser = spawn(process.env.CHITON_CHROMIUM || 'chromium', [
            '--headless=new', '--no-sandbox', '--disable-dev-shm-usage', '--no-first-run',
            '--no-default-browser-check', '--disable-background-networking', '--remote-debugging-pipe',
            '--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
            '--user-data-dir=' + profile,
            '--proxy-server=' + origin, '--proxy-bypass-list=127.0.0.1;localhost',
        ], { stdio: ['ignore', 'ignore', 'pipe', 'pipe', 'pipe'] });
        browser.stderr.on('data', data => { failure = (failure + data).slice(-8000); });
        browser.on('error', error => {
            for (const entry of pending.values()) { clearTimeout(entry.timer); entry.reject(error); }
            pending.clear();
        });
        browser.stdio[4].on('data', data => {
            buffered += data.toString();
            let end;
            while ((end = buffered.indexOf('\0')) >= 0) {
                const message = JSON.parse(buffered.slice(0, end)); buffered = buffered.slice(end + 1);
                if (message.id) {
                    const entry = pending.get(message.id);
                    if (!entry) continue;
                    clearTimeout(entry.timer); pending.delete(message.id);
                    if (message.error) entry.reject(new Error(JSON.stringify(message.error)));
                    else entry.resolve(message.result);
                } else if (message.method === 'Browser.downloadWillBegin' || message.method === 'Browser.downloadProgress') {
                    downloadEvents.push(message);
                } else if (message.method === 'Fetch.requestPaused') {
                    const local = message.params.request.url.startsWith(origin + '/');
                    command(local ? 'Fetch.continueRequest' : 'Fetch.failRequest', {
                        requestId: message.params.requestId, ...(!local ? { errorReason: 'BlockedByClient' } : {}),
                    }, message.sessionId).catch(error => { if (!closing) exceptions.push(error.message); });
                } else if (message.method === 'Runtime.exceptionThrown') {
                    exceptions.push(message.params.exceptionDetails.exception?.description || message.params.exceptionDetails.text);
                }
            }
        });
        const target = await command('Target.createTarget', { url: 'about:blank' }, null);
        session = (await command('Target.attachToTarget', { targetId: target.targetId, flatten: true }, null)).sessionId;
        await command('Runtime.enable'); await command('Page.enable');
        await command('Fetch.enable', { patterns: [{ urlPattern: '*' }] });
        await command('Page.navigate', { url: origin + '/' });
        await wait('typeof stream_state === "function" && stream_state("frame").phase === "live" && stream_state("depth").phase === "live"');
        await evaluate('window.CHITON_INTERVAL = 200; GBI("interval").value = 5');
        assert.match(await evaluate('GBI("stream_status").textContent'), /host receipt age.*not exposure time/);
        // A different viewer changes the global device mode. This tab follows
        // without any mutation, and both selectors/display geometry agree.
        const mutations = counts['mode-mutation'] || 0;
        mockMode = 5;
        await wait('window.CHITON_MODE === 5 && stream_state("frame").phase === "live"');
        assert.deepEqual(await evaluate('[Number(GBI("colormode").value), Number(GBI("colormode2").value), window.CHITON_WIDTH]'), [5, 5, 1500]);
        assert.equal(counts['mode-mutation'] || 0, mutations);
        mockMode = 1;
        await wait('window.CHITON_MODE === 1 && stream_state("frame").phase === "live"');
        // Exercise actual native image decoding and failed JPEG recovery.
        behavior.image = 'invalid'; await wait('stream_state("frame").failures > 0');
        behavior.image = 'ok'; await wait('stream_state("frame").phase === "live"');
        await evaluate('GBI("colormode").value = "4"; colormode_change()');
        await wait('stream_state("frame").phase === "live" && window.CHITON_MODE == 4');
        const cloudMutations = counts['mode-mutation'];
        mockMode = 5;
        await wait('stream_state("frame").phase === "live" && window.CHITON_MODE === 5');
        mockMode = 4;
        await wait('stream_state("frame").phase === "live" && window.CHITON_MODE === 4');
        assert.equal(counts['mode-mutation'], cloudMutations);
        assert.equal(await evaluate('document.fullscreenElement === null'), true,
            'Global-mode following requested fullscreen');
        for (const state of ['error', 'invalid', 'disconnect']) {
            behavior.cloud = state; await wait('stream_state("frame").failures > 0');
            assert.equal(await evaluate('window.CHITON_READY'), true);
            behavior.cloud = 'ok'; await wait('stream_state("frame").phase === "live"');
        }
        for (const state of ['hang', 'hangBody']) {
            behavior.cloud = state;
            await wait('stream_state("frame").error === "Request timed out"', 13000);
            behavior.cloud = 'ok'; await wait('stream_state("frame").phase === "live"');
        }
        // Real stop/restart of HTTP while the page remains open.
        server.closeAllConnections(); await new Promise(resolve => server.close(resolve));
        await wait('stream_state("frame").failures > 0');
        mockFrame = 0; // Simulate the new process's frame sequence as well.
        await new Promise(resolve => server.listen(port, '127.0.0.1', resolve));
        await wait('stream_state("frame").phase === "live"');
        // Dispatch visibility transitions in Chromium to check native fetch cancellation.
        await evaluate('Object.defineProperty(document, "hidden", { configurable: true, value: true }); document.dispatchEvent(new Event("visibilitychange"))');
        await wait('!stream_state("frame").task && !stream_state("depth").task && !stream_state("temp").task');
        const paused = { ...counts };
        await new Promise(resolve => setTimeout(resolve, 700));
        assert.deepEqual(counts, paused);
        await evaluate('Object.defineProperty(document, "hidden", { configurable: true, value: false }); document.dispatchEvent(new Event("visibilitychange"))');
        await wait('stream_state("frame").phase === "live" && stream_state("depth").phase === "live"');
        for (const route of ['image', 'cloud', 'depth', 'temp']) assert.equal(maximum[route], 1, route + ' overlapped: ' + JSON.stringify(overlaps));
        await require('./web_native_downloads.js')({command, evaluate, events: downloadEvents,
            directory: path.join(profile, 'downloads'), raw, obj,
            setExportFailure: value => { exportFailure = value; }});
        assert.deepEqual(exceptions, []);
        console.log('Chromium viewer recovery and native download tests passed (local mock sensor; external traffic blocked)');
    } catch (error) {
        throw new Error(error.message + (failure ? '\nChromium stderr:\n' + failure : ''), { cause: error });
    } finally {
        closing = true;
        server.closeAllConnections(); server.close();
        if (browser?.pid && browser.exitCode === null && browser.signalCode === null) {
            const exited = new Promise(resolve => browser.once('exit', resolve));
            browser.kill('SIGTERM');
            const timeout = setTimeout(() => browser.kill('SIGKILL'), 2000);
            await exited; clearTimeout(timeout);
        }
        for (const entry of pending.values()) { clearTimeout(entry.timer); entry.reject(new Error('Browser closed')); }
        fs.rmSync(profile, { recursive: true, force: true });
    }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
