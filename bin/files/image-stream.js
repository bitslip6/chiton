/* chiton.jpeg.v1: persistent, credit-controlled JPEG transport. No mutations. */
(function (root) {
    'use strict';
    const MAX_JPEG = 2 * 1024 * 1024;
    function dispose(image) {
        if (image && typeof image.close === 'function') image.close();
        else if (image) image.src = '';
    }
    function decode(blob, signal) {
        if (typeof root.createImageBitmap === 'function') return root.createImageBitmap(blob);
        return new Promise((resolve, reject) => {
            const image = new root.Image(), url = root.URL.createObjectURL(blob);
            const finish = error => {
                signal.removeEventListener('abort', abort);
                image.onload = image.onerror = null; root.URL.revokeObjectURL(url);
                if (error) { image.src = ''; reject(error); } else resolve(image);
            };
            const abort = () => finish(new Error('Stream decode cancelled'));
            image.onload = () => finish(null);
            image.onerror = () => finish(new Error('Invalid stream JPEG'));
            signal.addEventListener('abort', abort, {once:true});
            if (signal.aborted) abort(); else image.src = url;
        });
    }
    class ChitonImageStream {
        constructor(options) {
            this.options = options; this.socket = null; this.generation = 0; this.revision = 0;
            this.retryAt = 0; this.pending = null; this.busy = false; this.raf = null; this.timer = null;
            this.lastDraw = -Infinity; this.displayTimes = [];
            this.stats = {received:0, decoded:0, displayed:0, dropped:0, decode_ms:0, display_fps:0};
        }
        invalidate() {
            this.revision++;
            if (this.pending) { dispose(this.pending.image); this.pending = null; this.stats.dropped++; }
        }
        stop() {
            this.generation++; this.invalidate();
            if (this.decodeController) this.decodeController.abort();
            clearTimeout(this.timer); this.timer = null;
            if (this.raf !== null) root.cancelAnimationFrame(this.raf);
            this.raf = null;
            const socket = this.socket; this.socket = null;
            if (socket) { socket.onmessage = socket.onerror = socket.onclose = socket.onopen = null; socket.close(); }
        }
        fail(error) {
            this.stop(); this.retryAt = Date.now() + 10000;
            this.options.onError(error);
        }
        arm() {
            clearTimeout(this.timer);
            this.timer = setTimeout(() => this.fail(new Error('Image stream timed out')), 8000);
        }
        start() {
            if (this.socket) return true;
            if (this.busy || Date.now() < this.retryAt || typeof root.WebSocket !== 'function') return false;
            const generation = ++this.generation;
            try {
                this.socket = new root.WebSocket((root.location.protocol === 'https:' ? 'wss://' : 'ws://') +
                    root.location.host + '/stream/camera', 'chiton.jpeg.v1');
                this.socket.binaryType = 'arraybuffer';
                this.socket.onmessage = event => this.message(event.data, generation);
                this.socket.onerror = () => { if (generation === this.generation) this.fail(new Error('Image stream unavailable; using snapshots')); };
                this.socket.onclose = () => { if (generation === this.generation) this.fail(new Error('Image stream disconnected; using snapshots')); };
                this.arm(); return true;
            } catch (error) { this.fail(error); return false; }
        }
        async message(data, generation) {
            if (generation !== this.generation) return;
            this.arm();
            let decoding = false;
            try {
                if (typeof data === 'string') {
                    if (data.length > 16384) throw new Error('Oversized stream status');
                    const status = JSON.parse(data);
                    if (!Number.isInteger(status.mode) || status.mode < 0 || status.mode > 6 ||
                        typeof status.capture_enabled !== 'boolean') throw new Error('Invalid stream status');
                    this.options.onStatus(status); return;
                }
                if (!(data instanceof ArrayBuffer) || data.byteLength < 8 || data.byteLength > MAX_JPEG + 4100)
                    throw new Error('Invalid stream packet size');
                if (this.busy) throw new Error('Server exceeded stream credit');
                const length = new DataView(data).getUint32(0);
                if (!length || length > 4096 || length + 6 > data.byteLength) throw new Error('Invalid stream metadata length');
                const meta = JSON.parse(new TextDecoder().decode(new Uint8Array(data, 4, length)));
                if (meta.version !== 1 || !Number.isSafeInteger(meta.frame_id) || meta.frame_id < 1 ||
                    !Number.isInteger(meta.mode) || meta.mode < 0 || meta.mode > 6 || meta.mode === 4 ||
                    !Number.isFinite(meta.age_ms) || meta.age_ms < 0 ||
                    !Number.isInteger(meta.width) || !Number.isInteger(meta.height) ||
                    !((meta.width === 640 && meta.height === 480) || (meta.width === 1500 && meta.height === 165)))
                    throw new Error('Invalid stream frame metadata');
                const bytes = new Uint8Array(data, 4 + length);
                if (bytes.length > MAX_JPEG || bytes[0] !== 255 || bytes[1] !== 216) throw new Error('Missing stream JPEG marker');
                this.busy = true; decoding = true; this.stats.received++;
                const revision = this.revision, started = Date.now();
                this.decodeController = new AbortController();
                const image = await decode(new Blob([bytes], {type:'image/jpeg'}), this.decodeController.signal);
                this.busy = false;
                if (generation !== this.generation) { dispose(image); return; }
                this.stats.decoded++; this.stats.decode_ms = Date.now() - started;
                if ((image.width || image.naturalWidth) !== meta.width || (image.height || image.naturalHeight) !== meta.height) {
                    dispose(image); throw new Error('Stream JPEG dimensions disagree with metadata');
                }
                // Return credit after decoding, not after paint: RTT/RAF do not
                // build an unbounded decoder queue. Only one decoded image waits.
                this.socket.send(String(meta.frame_id));
                if (revision !== this.revision) { dispose(image); this.stats.dropped++; return; }
                if (this.pending) { dispose(this.pending.image); this.stats.dropped++; }
                this.pending = {image, frame:{id:meta.frame_id, mode:meta.mode, age:meta.age_ms,
                    received:started, configuration_revision:meta.configuration_revision}};
                if (this.raf === null) this.raf = root.requestAnimationFrame(time => this.paint(time));
            } catch (error) {
                if (decoding) this.busy = false;
                if (generation === this.generation) this.fail(error);
            }
        }
        paint(time) {
            this.raf = null;
            if (!this.pending || !this.socket) return;
            const fps = Math.max(1, Math.min(30, Number(this.options.fps()) || 30));
            if (time - this.lastDraw + 0.5 < 1000/fps) {
                this.raf = root.requestAnimationFrame(next => this.paint(next)); return;
            }
            const pending = this.pending; this.pending = null;
            try {
                if (this.options.onFrame(pending.image, pending.frame) !== false) {
                    this.lastDraw = time; this.stats.displayed++;
                    this.displayTimes.push(time);
                    while (this.displayTimes.length > 120 || (this.displayTimes.length > 1 && this.displayTimes[0] < time-2000)) this.displayTimes.shift();
                    const elapsed = time-this.displayTimes[0];
                    this.stats.display_fps = elapsed > 0 ? (this.displayTimes.length-1)*1000/elapsed : 0;
                } else this.stats.dropped++;
            } catch (error) { this.fail(error); }
            finally { dispose(pending.image); }
        }
    }
    // Independent diagnostic reads also work in polling/cloud modes. One request
    // at a time, bounded deadline, no history, and no work while paused.
    class ChitonMetricsReader {
        constructor(onMetrics) { this.onMetrics = onMetrics; this.controller = null; this.updatedAt = 0; this.metrics = null; }
        stop() { if (this.controller) this.controller.abort(); }
        async poll(paused) {
            if (paused) { this.stop(); return; }
            if (this.controller || typeof root.fetch !== 'function') return;
            const controller = new AbortController(); this.controller = controller;
            const timer = setTimeout(() => controller.abort(), 1500);
            try {
                const response = await root.fetch('/stream/metrics', {cache:'no-store', signal:controller.signal});
                if (!response.ok) throw new Error('Metrics unavailable');
                const metrics = await response.json();
                if (!controller.signal.aborted && metrics && metrics.pipeline) {
                    this.metrics = metrics; this.updatedAt = Date.now(); this.onMetrics(metrics);
                }
            } catch (_) { /* stale state is visible; metrics failure never changes transport */ }
            finally { clearTimeout(timer); if (this.controller === controller) this.controller = null; }
        }
    }
    function formatDiagnostics(metrics) {
        const p = metrics && metrics.pipeline, system = p && p.system, stages = p && p.stages;
        const number = (v, suffix) => typeof v === 'number' && Number.isFinite(v) ? v.toFixed(2) + suffix : 'unavailable';
        let text = '';
        if (system) {
            text += ' | CPU process/camera ' + number(system.process_cpu_percent, '%') + '/' + number(system.camera_cpu_percent, '%');
            text += '; core ' + (system.camera_last_core == null ? 'unavailable' : system.camera_last_core);
            text += '; aggregate ' + number(system.aggregate_cpu_busy_percent, '%');
            text += '; per-core ' + ((system.per_core || []).map(c => c.core + ':' + number(c.busy_percent, '%') +
                '@' + number(c.frequency_khz, 'kHz')).join(',') || 'unavailable');
            text += '; temperature ' + ((system.thermal_zones || []).map(t => t.type + ':' + number(t.celsius, '°C')).join(',') || 'unavailable');
            text += '; throttle ' + (system.throttled == null ? 'unknown' : String(system.throttled));
            text += '; cooling ' + ((system.cooling_devices || []).map(c => c.type + ':' + c.state).join(',') || 'unavailable');
            text += '; monitor ' + number(system.collection_ms, 'ms');
        }
        if (stages) {
            text += ' | stage mean/p95≤/p99≤ (ms), mode ' + stages.mode + ', epoch ' + stages.epoch + ': ';
            text += ['processing', 'calibration_smoothing', 'preview_copy', 'conversion'].map(name => {
                const s = stages[name] || {};
                return name + ' ' + number(s.mean_ms, '') + '/' + number(s.p95_ms_upper_bound, '') + '/' + number(s.p99_ms_upper_bound, '');
            }).join('; ');
            text += '; register mean ' + number(stages.register_io && stages.register_io.mean_ms, 'ms');
            text += '; timer-pair probe/record overhead ' + number(stages.timer_probe && stages.timer_probe.mean_ms, 'ms') +
                '/' + number(stages.record_overhead && stages.record_overhead.mean_ms, 'ms');
        }
        if (metrics) text += ' | clients ' + metrics.clients;
        return text;
    }
    root.ChitonMetricsReader = ChitonMetricsReader;
    root.chitonFormatDiagnostics = formatDiagnostics;
    root.ChitonImageStream = ChitonImageStream;
})(window);
