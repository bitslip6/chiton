// Firmware selection tests use mocked fetch only; never install an upgrade.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

module.exports = async function testFirmware(path) {
    const html = fs.readFileSync(path, 'utf8');
    const start = html.indexOf('        function software_change(');
    const end = html.indexOf('        function calibration_enable(', start);
    assert(start >= 0 && end > start);
    assert(!html.includes('software_ver2'));
    assert.match(html, /<option value="" selected="selected">Choose version<\/option>/);

    let selector = {value: ''};
    let ok = true, networkFailure = false;
    const requests = [], messages = [], alerts = [];
    const context = vm.createContext({
        GBI: id => { assert.equal(id, 'software_ver'); return selector; },
        add_info: (type, message) => messages.push({type, message}),
        alert: message => alerts.push(message),
        fetch: url => {
            requests.push(url);
            if (networkFailure) return Promise.reject(new Error('network unavailable'));
            return Promise.resolve({ok, status: ok ? 200 : 501,
                json: () => Promise.resolve(ok ? {upgrade: Number(url.split('/').pop())} :
                    {error: 'software upgrade is unavailable with the camera simulator'})});
        },
    });
    const responseStart = html.indexOf('        function read_json_response(');
    const responseEnd = html.indexOf('        function apply_frame_type(', responseStart);
    vm.runInContext(html.slice(responseStart, responseEnd) + html.slice(start, end), context);

    for (const value of ['', 'Choose version', '31', '100', 'bogus', 'NaN', 'Infinity',
                         '74junk', '74.5', '7e1', '0x4a', '+74', '-74', ' 74', '74 ',
                         '074', '999999999999999999999999', '74/extra']) {
        selector.value = value;
        messages.length = 0;
        assert.equal(context.software_change(), false, value);
        assert.equal(requests.length, 0, value);
        assert.equal(alerts.length, 0, value);
        assert.equal(messages.length, 1, value);
        assert.equal(messages[0].type, 'danger');
        assert.match(messages[0].message, /32 and 99/);
    }

    selector = null;
    assert.equal(context.software_change(), false);
    assert.equal(requests.length, 0);
    selector = {value: ''};

    for (const value of ['32', '74', '99']) {
        selector.value = value;
        const count = requests.length;
        await context.software_change();
        assert.equal(requests.length, count + 1);
        assert.equal(requests.at(-1), '/upgrade/software/' + value);
        assert.match(alerts.at(-1), new RegExp('system upgraded to: ' + value));
    }

    // Valid selections still surface HTTP/network failures without retries or
    // success alerts. These simulated responses perform no device operation.
    const alertCount = alerts.length;
    ok = false;
    let count = requests.length;
    await context.software_change();
    assert.equal(requests.length, count + 1);
    assert.equal(alerts.length, alertCount);
    assert.equal(messages.at(-1).type, 'danger');
    assert.match(messages.at(-1).message, /unavailable/);

    networkFailure = true;
    count = requests.length;
    await context.software_change();
    assert.equal(requests.length, count + 1);
    assert.equal(alerts.length, alertCount);
    assert.match(messages.at(-1).message, /network unavailable/);
};
