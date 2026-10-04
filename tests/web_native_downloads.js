// Native Chromium download capture, using only the browser suite's local mock.
// Production export bytes/headers are independently checked by http_exports.py.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

module.exports = async function testNativeDownloads({command, evaluate, events, directory,
                                                     raw, obj, setExportFailure}) {
    fs.mkdirSync(directory, {recursive: true});
    await command('Browser.setDownloadBehavior', {
        behavior: 'allowAndName', downloadPath: directory, eventsEnabled: true,
    }, null);

    async function download(id, filename) {
        const before = events.filter(event => event.method === 'Browser.downloadWillBegin').length;
        await evaluate(`GBI(${JSON.stringify(id)}).click()`, true);
        const deadline = Date.now() + 6000;
        let started;
        while (Date.now() < deadline) {
            started = events.filter(event => event.method === 'Browser.downloadWillBegin')[before];
            if (started && events.some(event => event.method === 'Browser.downloadProgress' &&
                event.params.guid === started.params.guid && event.params.state === 'completed')) break;
            await new Promise(resolve => setTimeout(resolve, 50));
        }
        assert(started, id + ': no native download started');
        assert.equal(started.params.suggestedFilename, filename);
        assert(events.some(event => event.method === 'Browser.downloadProgress' &&
            event.params.guid === started.params.guid && event.params.state === 'completed'),
            id + ': native download did not complete');
        // Allow time for an accidental duplicate listener/export to surface.
        await new Promise(resolve => setTimeout(resolve, 200));
        assert.equal(events.filter(event => event.method === 'Browser.downloadWillBegin').length, before + 1);
        return fs.readFileSync(path.join(directory, started.params.guid));
    }

    // Freeze the actual rendered canvas so the native saved file can be compared
    // byte-for-byte with its PNG encoding, not merely its extension or MIME type.
    await evaluate('streams_stopped = true; pause_streams()');
    const pngURL = await evaluate('canvas_elm.toDataURL("image/png")');
    const pageURL = await evaluate('location.href');
    const png = await download('download_png', 'chiton.png');
    assert.deepEqual(png, Buffer.from(pngURL.split(',')[1], 'base64'));
    assert.deepEqual(png.subarray(0, 8), Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]));
    assert.equal(await evaluate('location.href'), pageURL, 'PNG link navigated the viewer');
    const base64 = JSON.stringify(png.toString('base64'));
    const decoded = await evaluate(`(async () => {
        const bytes = Uint8Array.from(atob(${base64}), c => c.charCodeAt(0));
        const image = await createImageBitmap(new Blob([bytes], {type: "image/png"}));
        const result = [image.width, image.height, canvas_elm.width, canvas_elm.height];
        image.close(); return result;
    })()`);
    assert.deepEqual(decoded.slice(0, 2), decoded.slice(2));
    assert.equal(png.readUInt32BE(16), decoded[0]);
    assert.equal(png.readUInt32BE(20), decoded[1]);

    assert.deepEqual(await download('raw_link', 'chiton-raw.bin'), raw);
    assert.deepEqual(await download('download_pointcloud', 'chiton-pointcloud.obj'), obj);

    // A JSON error without attachment disposition must not silently become a
    // bogus downloaded .bin/.obj file. New-tab error display is existing behavior.
    setExportFailure(true);
    const count = events.filter(event => event.method === 'Browser.downloadWillBegin').length;
    for (const id of ['raw_link', 'download_pointcloud']) {
        await evaluate(`GBI(${JSON.stringify(id)}).click()`, true);
        await new Promise(resolve => setTimeout(resolve, 500));
    }
    assert.equal(events.filter(event => event.method === 'Browser.downloadWillBegin').length, count);
    setExportFailure(false);
    assert.equal(fs.readdirSync(directory).length, 3);
};
