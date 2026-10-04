// Dependency-free tests of the shipped UI's settings and live-stream recovery.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

async function test(path) {
    const html = fs.readFileSync(path, 'utf8');
    const script = html.slice(html.indexOf('        function read_json_response('),
                              html.indexOf('        function slam_click(')) +
                   html.slice(html.indexOf('        function apply_color_range('),
                              html.indexOf('        function gain_change('));
    assert(script.includes('function depth_click'));
    const checkbox = { checked: false, disabled: false };
    const minimum = { value: 0, disabled: false }, maximum = { value: 6500, disabled: false };
    const controls = {depth: checkbox, min_val: minimum, max_val: maximum};
    const requests = [], messages = [];
    let result = { value: true, frame_type: 'depth' }, ok = true;
    const context = vm.createContext({
        window: {},
        GBI: name => { assert(controls[name]); return controls[name]; },
        add_info: (type, message) => messages.push({ type, message }),
        fetch: url => {
            requests.push(url);
            return Promise.resolve({ ok, status: ok ? 200 : 500, json: () => Promise.resolve(result) });
        },
    });
    vm.runInContext(script, context);
    await context.sync_frame_type();
    assert.equal(requests.pop(), '/depth/setting');
    assert.equal(checkbox.checked, false);
    assert.equal(context.window.CHITON_FRAME_TYPE, 'depth');
    checkbox.checked = true;
    result = { value: false, frame_type: 'ir' };
    const selecting = context.depth_click();
    assert.equal(checkbox.disabled, true);
    await selecting;
    assert.equal(requests.pop(), '/depth/setting/ir');
    assert.equal(checkbox.checked, true);
    assert.equal(checkbox.disabled, false);
    assert.equal(messages.at(-1).type, 'success');
    // Error status restores the acknowledged type rather than leaving a lying control.
    checkbox.checked = false;
    ok = false; result = { error: 'camera unavailable' };
    await context.depth_click();
    assert.equal(requests.pop(), '/depth/setting/depth');
    assert.equal(checkbox.checked, true);
    assert.equal(checkbox.disabled, false);
    assert.equal(messages.at(-1).type, 'danger');
    assert(messages.at(-1).message.includes('camera unavailable'));
    ok = true; result = { value: true, frame_type: 'depth' };
    checkbox.checked = false;
    await context.depth_click();
    assert.equal(checkbox.checked, false);
    assert.equal(context.window.CHITON_FRAME_TYPE, 'depth');
    result = { frame_type: 'unknown' };
    await assert.rejects(context.sync_frame_type(), /Invalid frame type/);
    result = { min: 100, max: 200 };
    await context.sync_color_range();
    assert.equal(requests.pop(), '/range/setting');
    assert.equal(minimum.value, 100);
    assert.equal(maximum.value, 200);
    minimum.value = 200;
    const count = requests.length;
    await context.min_change();
    assert.equal(requests.length, count); // Invalid pair never reaches the API.
    assert.equal(minimum.value, 100);
    assert.equal(messages.at(-1).type, 'danger');
    maximum.value = 400; result = { value: 400, min: 100, max: 400 };
    await context.max_change();
    assert.equal(requests.pop(), '/range/setting/100/400');
    assert.equal(maximum.value, 400);
    minimum.value = 150; ok = false; result = { error: 'min must be less than max' };
    await context.min_change();
    assert.equal(requests.pop(), '/range/setting/150/400');
    assert.equal(minimum.value, 100);
    assert.equal(maximum.value, 400);
    assert.equal(minimum.disabled, false);
    assert.equal(maximum.disabled, false);
    assert.equal(messages.at(-1).type, 'danger');
}
(async () => {
    for (const path of ['release/files/app.html', 'bin/files/app.html']) {
        await test(path);
        await require('./web_contracts.js')(path);
        require('./web_downloads.js')(path);
        await require('./web_firmware.js')(path);
        await require('./web_streams.js')(path);
    }
    assert.equal(fs.readFileSync('release/files/app.html', 'utf8'),
                 fs.readFileSync('bin/files/app.html', 'utf8'));
    console.log('Web UI tests passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
