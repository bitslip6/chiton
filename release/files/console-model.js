/* Pure console data contracts. All uint16 values participate; no validity mask. */
(function (root) {
    'use strict';
    const C = root.ChitonConsole = root.ChitonConsole || {};
    const finite = (v, name, min = -Infinity, max = Infinity) => {
        if (typeof v !== 'number' || !Number.isFinite(v) || v < min || v > max) throw new Error('Invalid ' + name);
        return v;
    };
    const integer = (v, name, min = 0, max = Number.MAX_SAFE_INTEGER) => {
        finite(v, name, min, max); if (!Number.isSafeInteger(v)) throw new Error('Invalid ' + name); return v;
    };
    const optional = (v, check) => v == null ? null : check(v);
    const category = (v, choices, name) => { if (!choices.includes(v)) throw new Error('Invalid ' + name); return v; };
    const boolean = (v, name) => { if (typeof v !== 'boolean') throw new Error('Invalid ' + name); return v; };
    function selection(s, width = 640, height = 480) {
        category(s.kind, ['point', 'roi'], 'selection');
        const out = {kind:s.kind, x:integer(s.x, 'X', 0, width-1), y:integer(s.y, 'Y', 0, height-1)};
        if (s.kind === 'roi') {
            out.x2 = integer(s.x2, 'ROI X2', out.x+1, width);
            out.y2 = integer(s.y2, 'ROI Y2', out.y+1, height);
        }
        return out;
    }
    function normalize(p, provenance) {
        if (!p || typeof p !== 'object') throw new Error('Invalid measurement');
        const units = category(p.units, ['mm','depth_sensor_units','ir_intensity_units'], 'units');
        const frameType = category(p.frame_type, ['depth','ir'], 'frame type');
        if ((frameType === 'ir') !== (units === 'ir_intensity_units')) throw new Error('Inconsistent units/type');
        const width = integer(p.width, 'width', 1, 1500), height = integer(p.height, 'height', 1, 480);
        if (!((width === 640 && height === 480) || (width === 1500 && height === 165))) throw new Error('Unsupported dimensions');
        const out = {source:provenance, frameId:integer(p.frame_id,'frame ID',1),
            capturedAtMs:optional(p.captured_at_ms, v => finite(v,'timestamp',1)),
            frameAgeMs:optional(p.frame_age_ms, v => finite(v,'frame age',0)),
            revision:integer(p.configuration_revision,'revision'), frameType, units,
            unitsScope:optional(p.units_scope, v => category(v,['calibration_aoi','full_sensor'],'scope')),
            mode:integer(p.color_mode,'mode',0,6), width, height,
            statisticsKind:category(p.statistics_kind,['spatial','temporal_observed_frames'],'statistics kind'),
            value:integer(p.value,'value',0,65535), mean:finite(p.mean,'mean',0,65535),
            stddev:finite(p.stddev,'stddev',0,65535), min:optional(p.min,v=>integer(v,'min',0,65535)),
            max:optional(p.max,v=>integer(v,'max',0,65535)), range:finite(p.diff,'range',0,65535),
            count:integer(p.sample_count,'count',1,640*480),
            firstTime:optional(p.first_captured_at_ms,v=>finite(v,'history start',1)),
            lastTime:optional(p.last_captured_at_ms,v=>finite(v,'history end',1)),
            historyReset:p.history_reset == null ? false : boolean(p.history_reset,'history reset'),
            calibrated:optional(p.calibration_applied,v=>boolean(v,'calibration')),
            smoothed:optional(p.smoothed,v=>boolean(v,'smoothing')),
            plugin:optional(p.plugin_applied,v=>boolean(v,'plugin')),
            rms:optional(p.rms_about_zero,v=>finite(v,'RMS',0,65535)),
            trimmed:optional(p['1std_mean'],v=>finite(v,'one sigma mean',0,65535))};
        if (out.statisticsKind === 'temporal_observed_frames' && out.count > 60) throw new Error('Unsupported temporal count');
        if (out.min != null && out.max != null && out.min > out.max) throw new Error('Invalid extrema');
        if (out.firstTime != null && out.lastTime != null && out.lastTime < out.firstTime) throw new Error('Invalid history span');
        if (p.invalid_sample_policy != null && p.invalid_sample_policy !== 'all_uint16_included') throw new Error('Unknown validity policy');
        return out;
    }
    function stats(values) {
        if (!values.length) return null;
        const sorted = [...values].sort((a,b)=>a-b), n = sorted.length;
        const quantile = q => { const i=(n-1)*q, low=Math.floor(i); return sorted[low]+(sorted[Math.ceil(i)]-sorted[low])*(i-low); };
        const mean = values.reduce((a,b)=>a+b,0)/n;
        const stddev = Math.sqrt(values.reduce((s,v)=>s+(v-mean)**2,0)/n), median=quantile(.5);
        const deviations = values.map(v=>Math.abs(v-median)).sort((a,b)=>a-b);
        return {count:n, mean, stddev, min:sorted[0], max:sorted[n-1], range:sorted[n-1]-sorted[0], median,
            mad:(deviations[Math.floor((n-1)/2)]+deviations[Math.ceil((n-1)/2)])/2, p05:quantile(.05), p95:quantile(.95)};
    }
    class Ring {
        constructor(capacity = 300) { this.capacity = capacity; this.items = []; }
        push(item) { this.items.push(item); if (this.items.length > this.capacity) this.items.shift(); }
        clear() { this.items.length=0; }
        window(now, ms = 2000, cap = 60) { return this.items.filter(v => v.received >= now-ms).slice(-cap); }
    }
    function context(m,s) { return JSON.stringify([m.source,s,m.mode,m.frameType,m.units,m.unitsScope,m.revision,m.calibrated,m.smoothed,m.plugin]); }
    function mapPoint(clientX, clientY, rect, width, height, edge = false) {
        const scale=Math.min(rect.width/width,rect.height/height), w=width*scale,h=height*scale;
        const x=(clientX-rect.left-(rect.width-w)/2)/scale, y=(clientY-rect.top-(rect.height-h)/2)/scale;
        if (x<0 || y<0 || x>w/scale || y>h/scale) return null;
        return {x:Math.max(0,Math.min(width-(edge?0:1),Math.floor(x))),y:Math.max(0,Math.min(height-(edge?0:1),Math.floor(y)))};
    }
    function health(m, now, hz, phase, capture) {
        if (phase==='paused') return 'Paused';
        if (capture === false) return 'Capture disabled';
        if (!m) return phase==='error' ? 'Disconnected' : 'Connecting';
        const age=m.frameAgeMs == null ? null : m.frameAgeMs+now-m.received;
        if (now-m.received>8000) return 'Disconnected';
        if (phase==='error' || now-(m.distinctReceived??m.received)>Math.max(500,3000/hz) || (age!=null && age>Math.max(500,3000/hz))) return 'Delayed / stale';
        return m.count<10 && m.statisticsKind==='temporal_observed_frames' ? 'Warming up' : 'Live';
    }
    function csvCell(v) { let s=v==null?'':String(v); if (/^[\s]*[=+@-]/.test(s) && typeof v !== 'number') s="'"+s; return '"'+s.replaceAll('"','""')+'"'; }
    function csv(records) {
        const keys=['source','frameId','capturedAtMs','revision','mode','frameType','selection','units','unitsScope','value','estimate','segment','q','R','statisticsKind','count','mean','stddev','min','max','range'];
        return keys.join(',')+'\r\n'+records.map(row=>keys.map(k=>csvCell(k==='selection'?JSON.stringify(row[k]):row[k])).join(',')).join('\r\n');
    }
    const format = (v,digits=1) => v==null || !Number.isFinite(v) ? '—' : v.toLocaleString('en-US',{maximumFractionDigits:digits,minimumFractionDigits:0});
    const unitLabel = u => ({mm:'mm',depth_sensor_units:'sensor units',ir_intensity_units:'IR units'}[u] || 'units unknown');
    Object.assign(C,{finite,integer,category,boolean,selection,normalize,stats,Ring,context,mapPoint,health,csv,format,unitLabel});
    if (typeof module==='object') module.exports=C;
})(typeof window==='object'?window:globalThis);
