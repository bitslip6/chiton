#!/usr/bin/env python3
"""Console integration with an explicitly SDK-free CMake build; localhost only."""
from array import array
import http.client
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time
from runtime_support import runtime_directory, simulator_environment

ROOT = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1] if len(sys.argv) > 1 else '_build/console-stub/chiton').resolve()
cache = binary.parent / 'CMakeCache.txt'
assert cache.is_file() and re.search(r'^USE_ADITOF_STUB:BOOL=ON$', cache.read_text(), re.M), 'Refusing anything except an explicit CMake SDK-free simulator build'
assets = ROOT / 'release/files'
assert b'file-backed camera: hardware temperature monitor disabled' in binary.read_bytes(), 'Refusing binary without simulator signature'
out = Path(os.environ.get('CHITON_CONSOLE_NATIVE_OUT', str(ROOT / '_build/console-review/native'))).resolve()
out.mkdir(parents=True, exist_ok=True)
with runtime_directory('chiton-console-native-') as directory:
    root = Path(directory)
    # Exact resource override prevents installed/executable plugins or state from leaking in.
    shutil.copyfile(ROOT / 'release/chiton.json', root / 'chiton.json')
    data = array('H', [800]) * (640 * 480)
    if sys.byteorder != 'little':
        data.byteswap()
    (root / 'depth.bin').write_bytes(data.tobytes())
    (root / 'ir.bin').write_bytes(data.tobytes())
    env = simulator_environment(directory)
    env.update(CHITON_RESOURCE_DIR=str(root), CHITON_ASSET_DIR=str(assets),
               ADITOF_STUB_DEPTH=str(root / 'depth.bin'), ADITOF_STUB_IR=str(root / 'ir.bin'),
               CHITON_CALIBRATION_FILE=str(root / 'calibration.bin'))
    with (out / 'runtime.log').open('w') as log:
        process = subprocess.Popen([str(binary), '--display', 'headless'], cwd=root, env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 20
            match = None
            while time.monotonic() < deadline and process.poll() is None:
                text = (out / 'runtime.log').read_text()
                match = re.search(r'web server listening on port (\d+)', text)
                if match:
                    break
                time.sleep(.05)
            assert match and process.poll() is None, (out / 'runtime.log').read_text()
            assert 'file-backed camera: hardware temperature monitor disabled' in text, 'Not a verified simulator'
            assert 'process_standard_frame callback found' not in text
            port = int(match[1])
            def get(route, expected=200, method='GET', with_headers=False):
                connection = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
                try:
                    connection.request(method, route)
                    response = connection.getresponse()
                    body = response.read()
                    assert response.status in ((expected,) if isinstance(expected, int) else expected), (route, response.status, body[:200])
                    return (body, {k.lower():v for k,v in response.getheaders()}) if with_headers else body
                finally:
                    connection.close()
            # Simulator-only benign configuration, never executed before identity check.
            for route in ['/chitoncal/setting/off', '/smoothing/setting/off', '/depth/setting/depth', '/colormode/setting/1']:
                get(route)
            html_bytes = get('/console.html')
            assert html_bytes == (assets / 'console.html').read_bytes(), 'console.html'
            html = html_bytes.decode()
            names = re.findall(r'(?:src|href)="/([^"?#]+)"', html)
            for name in names:
                assert get('/' + name) == (assets / name).read_bytes(), name
            assert get('/app.html') == (assets / 'app.html').read_bytes()
            from http_exports import exercise
            exercise(get)  # Actual cloud/OBJ headers, signed bytes, /6000 numerical comparison.
            result = subprocess.run(['node', str(ROOT / 'tests/console_native_browser.js'), f'http://127.0.0.1:{port}'], cwd=ROOT, timeout=60)
            assert result.returncode == 0
            print(f'console_native: verified simulator, {len(names)} static routes and legacy bytes passed')
        finally:
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
