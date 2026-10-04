// Inventory every named, visible application control (ignore commented markup).
// This structural gate complements runtime settings/streams/download tests; it
// never contacts provisioning, upgrade, or diagnostic services.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const inventory = require('./ui_contracts.json');

module.exports = async function testContracts(path) {
    const html = fs.readFileSync(path, 'utf8');
    const uncommented = html.replace(/<!--[\s\S]*?-->/g, '');
    const markup = uncommented.replace(/<script\b[^>]*>[\s\S]*?<\/script>/g, '');
    const controls = new Map();
    for (const match of markup.matchAll(/<(input|select|button|a|canvas)\b([^>]*)>/g)) {
        const attributes = Object.fromEntries([...match[2].matchAll(/([\w-]+)="([^"]*)"/g)]
            .map(item => [item[1], item[2]]));
        if (!attributes.id) continue; // Bootstrap's unnamed navbar toggle is local navigation.
        assert(!controls.has(attributes.id), 'Duplicate control ID: ' + attributes.id);
        controls.set(attributes.id, {attributes, source: match[0]});
    }
    assert.deepEqual([...controls.keys()].sort(), inventory.map(row => row.id).sort());
    const script = uncommented.replace(/^\s*\/\/.*$/gm, '');
    const bindings = [...script.matchAll(/GBI\("([^"]+)"\)\.addEventListener\("([^"]+)", ([\s\S]*?), false\);/g)];
    for (const row of inventory) {
        const {attributes, source} = controls.get(row.id);
        assert(Array.isArray(row.routes) && row.result, row.id);
        if (row.kind === 'action') {
            const matches = bindings.filter(item => item[1] === row.id && item[2] === row.event);
            assert.equal(matches.length, 1, row.id + ' binding count');
            assert(matches[0][3].includes(row.handler), row.id + ' handler');
        } else if (row.kind === 'disabled') {
            assert(/\bdisabled\b/.test(source) && attributes.title.includes('unavailable'), row.id);
            assert(!bindings.some(item => item[1] === row.id), row.id);
        } else if (row.kind === 'field') {
            assert(controls.has(row.owner), row.id + ' owner');
        } else if (row.kind === 'link') {
            assert.equal(attributes.href, row.href);
        } else {
            assert.equal(row.kind, 'navigation');
            assert(attributes[row.event].includes(row.handler));
        }
    }
    const actions = inventory.filter(row => row.kind === 'action').map(row => row.id);
    assert(bindings.every(item => actions.includes(item[1])), 'Uninventoried binding');
    // Canvas release is the second half of the same local selection interaction.
    assert.equal(bindings.filter(item => item[1] === 'canvas' && item[2] === 'mouseup').length, 1);

    const functionText = uncommented.replace(/\/\*[\s\S]*?\*\//g, '');
    function functionSource(name) {
        const start = functionText.indexOf(`        function ${name}(`);
        assert(start >= 0, name);
        const bodyStart = functionText.indexOf('\n', start) + 1;
        const tail = functionText.slice(bodyStart);
        const next = /^\s*function \w+\(/m.exec(tail);
        assert(next, name);
        return functionText.slice(start, bodyStart + next.index);
    }
    const messages = [], requests = [], downloads = [];
    let ok = true, networkFailure = false, result = {value: true};
    const elements = {dynamic: {checked: true}, chitoncal: {checked: true}, smoothing: {checked: true},
        quality: {value: '75'}, gateway: {value: 'eth0'},
        wlan0_ssid: {value: 'mock-network'}, wlan0_pass: {value: 'mock-password'}};
    const context = vm.createContext({
        console: {log() {}},
        GBI: id => { assert(elements[id], id); return elements[id]; },
        add_info: (type, message) => messages.push({type, message}),
        fetch: url => {
            requests.push(url);
            if (networkFailure) return Promise.reject(new Error('mock disconnect'));
            return Promise.resolve({ok, status: ok ? 200 : 501,
                json: () => Promise.resolve(ok ? result : {error: 'unavailable in simulator'})});
        },
        document: {createElement: () => ({setAttribute(name, value) { this[name] = value; },
            click() { downloads.push({href: this.href, filename: this.download}); }})},
    });
    const names = ['read_json_response', 'dynamic_click', 'chitoncal_click', 'smoothing_click',
                   'toggle_option', 'quality_change', 'update_gateway', 'save_net', 'debuginfo'];
    vm.runInContext(names.map(functionSource).join('\n'), context);
    async function invoke(name) {
        assert.equal(typeof context[name], 'function', name);
        context[name]();
        for (let i = 0; i < 20; ++i) await Promise.resolve();
    }
    for (const id of ['dynamic', 'chitoncal', 'smoothing']) {
        for (const checked of [true, false]) {
            elements[id].checked = checked;
            result = {value: checked};
            await invoke(id + '_click');
            assert.equal(requests.at(-1), '/' + id + '/setting/' + (checked ? 'on' : 'off'));
            assert.equal(messages.at(-1).type, 'success');
        }
    }
    result = {value: 75}; await invoke('quality_change');
    assert.equal(requests.at(-1), '/quality/setting/75');
    assert.match(messages.at(-1).message, /75/);
    result = {retcode: 0}; await invoke('update_gateway');
    assert.equal(requests.at(-1), '/gateway/setting/eth0');
    assert.equal(messages.at(-1).type, 'success');
    result = {value: 'updated'}; await invoke('save_net');
    assert.equal(requests.at(-1), '/wlan0/setting/mock-network/mock-password');
    result = {value: 'mock-diagnostics.tar.gz'}; await invoke('debuginfo');
    assert.equal(requests.at(-1), '/debug/software');
    assert.deepEqual(downloads, [{href: result.value, filename: result.value}]);

    // Simulator rejection/network failures must not become success messages or
    // diagnostic downloads. Only mock requests are made, with no automatic retry.
    for (const failure of ['http', 'network']) {
        ok = false; networkFailure = failure === 'network';
        for (const name of ['dynamic_click', 'chitoncal_click', 'smoothing_click',
                            'quality_change', 'update_gateway', 'save_net', 'debuginfo']) {
            const count = requests.length, saved = downloads.length;
            messages.length = 0;
            await invoke(name);
            assert.equal(requests.length, count + 1, name);
            assert.equal(downloads.length, saved, name);
            assert.equal(messages.at(-1).type, 'danger', name);
        }
    }
};
