#!/usr/bin/env python3
"""F11 simulator-only measurement/API regressions. No hardware or system actions."""
from array import array
from contextlib import contextmanager
import http.client
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
from runtime_support import runtime_directory, simulator_environment

binary = Path(sys.argv[1] if len(sys.argv) > 1 else 'bin/chiton').resolve()
width, height = 640, 480

@contextmanager
def sensor(directory, samples, plugin=False):
    root = Path(directory)
    depth = array('H', samples)
    ir = array('H', [1800]) * (width * height)
    if sys.byteorder != 'little':
        depth.byteswap(); ir.byteswap()
    (root / 'depth.bin').write_bytes(depth.tobytes())
    (root / 'ir.bin').write_bytes(ir.tobytes())
    if plugin:
        source = root / 'plugin.cpp'
        source.write_text('''#include <stdint.h>
extern "C" void process_standard_frame(uint16_t *frame, float *, float *, uint16_t *) {
    frame[0] = frame[640 * 480 - 1]; // Expose the private legacy counter for verification.
    frame[2] = 2444;                // Ordinary plugin edits still publish.
    frame[640 * 480 - 1] = 9999;    // Sentinel edits must not replace a measurement.
}
''')
        subprocess.run([os.environ.get('CXX', 'g++'), '-shared', '-fPIC', str(source),
                        '-o', str(root / 'api.so')], check=True)
    env = simulator_environment(directory)
    env.update(ADITOF_STUB_DEPTH=str(root / 'depth.bin'), ADITOF_STUB_IR=str(root / 'ir.bin'),
               CHITON_CALIBRATION_FILE=str(root / 'calibration.bin'))
    log_path = root / 'runtime.log'
    with log_path.open('w') as log:
        process = subprocess.Popen([str(binary), '--display', 'headless'], cwd=root,
                                   env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 20
            match = None
            while time.monotonic() < deadline:
                text = log_path.read_text()
                match = re.search(r'web server listening on port (\d+)', text)
                if match or process.poll() is not None:
                    break
                time.sleep(.05)
            assert match and process.poll() is None, log_path.read_text()
            assert 'file-backed camera: hardware temperature monitor disabled' in text, \
                'Refusing to exercise a hardware build'
            port = int(match[1])
            def request(path, status=200):
                connection = http.client.HTTPConnection('127.0.0.1', port, timeout=12)
                try:
                    connection.request('GET', path)
                    response = connection.getresponse()
                    body = response.read()
                    assert response.status == status, (path, response.status, body[:200])
                    assert int(response.getheader('Content-Length')) == len(body)
                    headers = dict((name.lower(), value) for name, value in response.getheaders())
                    if 'application/json' in headers.get('content-type', ''):
                        body = json.loads(body)
                    return body, headers
                finally:
                    connection.close()
            request('/chitoncal/setting/off'); request('/smoothing/setting/off')
            deadline = time.monotonic() + 5
            while True:
                raw, headers = request('/raw/camera')
                words = array('H'); words.frombytes(raw)
                if headers['x-chiton-byte-order'] != sys.byteorder: words.byteswap()
                if words[-1] == samples[-1] and (plugin or words == array('H', samples)):
                    break
                assert time.monotonic() < deadline, 'Frame did not settle to the supplied fixture'
                time.sleep(.01)
            assert ('process_standard_frame callback found' in log_path.read_text()) == plugin, \
                'Unexpected installed/executable plugin; use an isolated simulator build'
            yield request
            process.send_signal(signal.SIGTERM)
            assert process.wait(timeout=5) == 0, log_path.read_text()
            assert 'threads joined' in log_path.read_text()
        finally:
            if process.poll() is None:
                process.kill(); process.wait()

def reference(values):
    mean = math.fsum(values) / len(values)
    deviation = math.sqrt(math.fsum((value - mean) ** 2 for value in values) / len(values))
    rms = math.sqrt(math.fsum(value * value for value in values) / len(values))
    return mean, deviation, rms

def check_roi(request, path, samples):
    result, _ = request(path)
    mean, deviation, rms = reference(samples)
    assert math.isclose(result['mean'], mean, abs_tol=1e-6), result
    assert math.isclose(result['stddev'], deviation, abs_tol=1e-6), result
    assert math.isclose(result['stdd'], deviation * 1000, abs_tol=.005), result
    assert math.isclose(result['rms'], deviation, abs_tol=1e-6), result
    assert math.isclose(result['rms_about_zero'], rms, abs_tol=1e-6), result
    assert result['diff'] == max(samples) - min(samples)
    assert result['sample_count'] == len(samples) and result['statistics_kind'] == 'spatial'
    assert 'temporal' not in result # Spatial spread must not be advertised as temporal noise.
    assert result['invalid_sample_policy'] == 'all_uint16_included'
    assert result['frame_id'] > 0 and result['captured_at_ms'] > 0
    assert result['units'] == 'depth_sensor_units'
    return result

patterns = {
    'constant': [800] * (width * height),
    'gradient': [4095 * (i % width) // (width - 1) for i in range(width * height)],
    'checkerboard': [4095 if (i // width // 40 + i % width // 40) % 2 else 0 for i in range(width * height)],
    'full-u16': [65535 if i % 2 else 0 for i in range(width * height)],
}
for name, samples in patterns.items():
    with runtime_directory(prefix='chiton-measurements-') as directory:
        with sensor(directory, samples) as request:
            check_roi(request, '/get/value/0/0/640/480', samples)
            check_roi(request, '/get/value/640/480/0/0', samples)
            roi = [samples[y * width + x] for y in range(22, 78) for x in range(17, 103)]
            check_roi(request, '/get/value/17/22/103/78', roi)
            for x, y in ((0, 0), (639, 479)):
                point, _ = request(f'/get/value/{x}/{y}')
                assert point['value'] == point['mean'] == samples[y * width + x]
                assert point['sample_count'] == 1 and point['stddev'] == point['diff'] == 0
            # Interleaved clients and point changes cannot contaminate each other's histories.
            seen = set()
            for _ in range(8):
                a, _ = request('/get/value/client/tab-A/0/0')
                seen.add(a['frame_id'])
                b, _ = request('/get/value/client/tab-B/639/479')
                assert a['mean'] == samples[0] and a['sample_count'] == len(seen) and a['stddev'] == 0
                assert b['mean'] == samples[-1] and b['stddev'] == 0
            changed, _ = request('/get/value/client/tab-A/639/479')
            assert changed['history_reset'] and changed['sample_count'] == 1 and changed['mean'] == samples[-1]
            check_roi(request, '/get/value/client/tab-A/0/0/640/480', samples)
            changed, _ = request('/get/value/client/tab-A/639/479')
            assert changed['history_reset'] and changed['sample_count'] == 1
            for path in ('/get/value/client', '/get/value/client/a/1',
                         '/get/value/client/a%2Fb/1/1', '/get/value/client/' + 'a' * 65 + '/1/1',
                         '/get/value/client/a/1/1/2', '/get/value/client/a/0/0/0/0'):
                request(path, 400)
            # Freeze publication explicitly: cloud reads are now observational.
            pause = Path(directory) / 'capture.pause'
            setting, _ = request('/colormode/setting/4')
            request('/pointcloud/camera?after=' + str(setting['frame_id']))
            pause.touch(); time.sleep(.05)
            first, _ = request('/get/value/client/frozen/10/10')
            duplicate, _ = request('/get/value/client/frozen/10/10')
            assert first['frame_id'] == duplicate['frame_id'] and duplicate['sample_count'] == 1
            pause.unlink()
            request('/pointcloud/camera?after=' + str(first['frame_id']))
            pause.touch(); time.sleep(.05)
            next_frame, _ = request('/get/value/client/frozen/10/10')
            assert next_frame['sample_count'] == 2 and next_frame['first_frame_id'] == first['frame_id']
            request('/depth/setting/ir')
            pause.unlink()
            request('/pointcloud/camera?after=' + str(next_frame['frame_id']))
            pause.touch(); time.sleep(.05)
            ir, _ = request('/get/value/client/frozen/10/10')
            assert ir['history_reset'] and ir['sample_count'] == 1 and ir['mean'] == 1800
            assert ir['units'] == 'ir_intensity_units' and ir['frame_type'] == 'ir'
            metadata, _ = request('/frame/camera')
            assert metadata['raw_layout'] == 'processed_uint16_row_major'
            raw, headers = request('/raw/camera')
            assert len(raw) == width * height * 2 and raw[-2:] == (1800).to_bytes(2, sys.byteorder)
            assert headers['x-chiton-units'] == 'ir_intensity_units'
            assert headers['x-chiton-layout'] == 'processed_uint16_row_major'
            assert headers['x-chiton-width'] == '640' and headers['x-chiton-height'] == '480'
            assert int(headers['x-chiton-frame-id']) == metadata['frame_id']
            request('/camera/setting/disable')
            request('/get/value/client/frozen/10/10', 503)
            request('/frame/camera', 503)
            request('/camera/setting/enable')
            pause.unlink()
    print(name + ': measurement HTTP tests passed')

with runtime_directory(prefix='chiton-measurements-plugin-') as directory:
    with sensor(directory, [800] * (width * height), plugin=True) as request:
        for _ in range(8):
            raw, headers = request('/raw/camera')
            words = array('H'); words.frombytes(raw)
            assert words[0] == (int(headers['x-chiton-frame-id']) - 1) % 65536
            assert words[2] == 2444 and words[-1] == 800
            last, _ = request('/get/value/639/479')
            assert last['value'] == 800 and last['plugin_applied'] is True
print('Legacy plugin compatibility and measurement HTTP tests passed')
