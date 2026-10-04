// F13 UI-only contracts: dormant handlers must not hit absent API routes, and
// one PNG click must generate one correctly named download without navigation.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

module.exports = function testDownloads(path) {
    const html = fs.readFileSync(path, 'utf8');

    function functionSource(name) {
        const start = html.indexOf(`        function ${name}(`);
        assert(start >= 0, name);
        const end = html.indexOf('        function ', start + 1);
        assert(end > start, name);
        return html.slice(start, end);
    }

    const unsupported = ['load_snr', 'gain_change', 'offset_change', 'upgrade',
                         'set_json_targets_distances'];
    const messages = [], exports = [], mimeTypes = [], listeners = [];
    const png = 'data:image/png;base64,iVBORw0KGgo=';
    const context = vm.createContext({
        window: {},
        fetch: () => assert.fail('Unsupported handler made an HTTP request'),
        setTimeout: () => assert.fail('Unsupported handler started polling'),
        add_info: (type, message) => messages.push({type, message}),
        canvas_elm: {toDataURL: type => { mimeTypes.push(type); return png; }},
        GBI: id => {
            assert.equal(id, 'download_png');
            return {addEventListener: (event, handler) => {
                assert.equal(event, 'click');
                listeners.push(handler);
            }};
        },
        document: {
            createElement: tag => {
                assert.equal(tag, 'a');
                return {dispatchEvent: function () {
                    exports.push({filename: this.download, href: this.href});
                }};
            },
            createEvent: type => {
                assert.equal(type, 'MouseEvents');
                return {initMouseEvent: () => {}};
            },
        },
    });
    context.window.setTimeout = context.setTimeout;
    vm.runInContext(functionSource('unsupported_control') +
                    unsupported.map(functionSource).join('\n') +
                    functionSource('dlCanvas') + functionSource('download'), context);

    for (const name of unsupported) {
        assert.equal(context[name](), false, name);
        assert.equal(messages.at(-1).type, 'warning');
        assert.match(messages.at(-1).message, /unavailable/);
    }
    for (const route of ['noise', 'gain', 'offset', 'version', 'targets']) {
        assert(!html.includes(`fetch("/${route}/setting/`), route);
    }

    assert.equal((html.match(/id="download_png"/g) || []).length, 1);
    assert.equal((html.match(/id="download_pointcloud"/g) || []).length, 1);
    assert(!html.includes('dwn_link'));
    assert.match(html, /id="download_png">Download PNG<\/a>/);
    assert.match(html, /<a href="\/pointcloud_obj\/camera"[^>]*id="download_pointcloud"/);
    assert(!html.includes('chiton.jpeg'));

    const bindings = [...html.matchAll(/GBI\("download_png"\)\.addEventListener\("click", e => \{[\s\S]*?\}, false\);/g)];
    assert.equal(bindings.length, 1);
    vm.runInContext(bindings[0][0], context);
    assert.equal(listeners.length, 1);

    let prevented = 0;
    listeners[0]({preventDefault: () => { ++prevented; }});
    assert.equal(prevented, 1);
    assert.deepEqual(exports, [{filename: 'chiton.png', href: png}]);
    assert.deepEqual(mimeTypes, ['image/png']);

    // Retain the legacy anchor helper with the same PNG type/name contract.
    const anchor = {};
    context.dlCanvas.call(anchor);
    assert.equal(anchor.href, png);
    assert.equal(anchor.download, 'chiton.png');

    // Exercise the existing old-browser download fallback as well.
    context.document.createEvent = undefined;
    context.document.createElement = () => ({fireEvent: function (event) {
        assert.equal(event, 'onclick');
        exports.push({filename: this.download, href: this.href});
    }});
    listeners[0]({preventDefault: () => { ++prevented; }});
    assert.equal(prevented, 2);
    assert.equal(exports.length, 2);
    assert.deepEqual(exports[1], {filename: 'chiton.png', href: png});
};
