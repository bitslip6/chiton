#!/usr/bin/env python3
"""Operator-run native simulator integration. Never execute ARM or real SDK builds."""
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
binary = Path(sys.argv[1]).resolve()
assert len(sys.argv) == 3 and sys.argv[2] in ('available','unavailable'), 'Specify compiled availability explicitly'
available = sys.argv[2] == 'available'
elf = binary.read_bytes()
assert elf[:4] == b'\x7fELF' and int.from_bytes(elf[18:20], 'little') == 62, 'Requires native x86-64 ELF; never execute ARM'
assert b'file-backed camera: hardware temperature monitor disabled' in elf, 'Refusing non-simulator executable BEFORE startup'

with runtime_directory('chiton-illumination-http-') as directory:
    env = simulator_environment(directory)
    for name in ('ADITOF_STUB_DEPTH', 'ADITOF_STUB_IR'):
        env[name] = str(Path(directory)/f'{name}.bin')
        Path(env[name]).write_bytes((200).to_bytes(2,'little') * (640*480))
    config = Path(directory)/'chiton.json'
    shutil.copy2(ROOT/'release/chiton.json', config)
    env.update(CHITON_CONFIG_FILE=str(config), CHITON_CALIBRATION_FILE=str(Path(directory)/'calibration.bin'),
               CHITON_ASSET_DIR=str(ROOT/'release/files'))
    # Invalid selections fail before workers/camera startup.
    invalid = [['--illumination-policy','experimental-depth-proxy'],
               ['--illumination-initial','1,1,1,1'],
               ['--illumination-policy','experimental-depth-proxy','--illumination-initial','0,1,1,1']]
    if not available:
        invalid.append(['--illumination-policy','experimental-depth-proxy','--illumination-initial','1,1,1,1'])
    for args in invalid:
        result = subprocess.run([str(binary),*args], cwd=directory, env=env, capture_output=True, timeout=10)
        assert result.returncode != 0 and b'camera running' not in result.stdout

    def run(experimental):
        log_path = Path(directory)/('experimental.log' if experimental else 'legacy.log')
        args = ['--display','headless']
        if experimental: args += ['--illumination-policy','experimental-depth-proxy','--illumination-initial','8,8,8,8']
        with log_path.open('w') as log:
            process = subprocess.Popen([str(binary),*args], cwd=directory, env=env, stdout=log, stderr=log)
            try:
                deadline = time.monotonic()+25
                match = None
                while time.monotonic() < deadline:
                    match = re.search(r'web server listening on port (\d+)',log_path.read_text())
                    if match or process.poll() is not None: break
                    time.sleep(.05)
                assert process.poll() is None and match, log_path.read_text()
                port = int(match[1])
                def request(path, expected=200):
                    connection = http.client.HTTPConnection('127.0.0.1',port,timeout=15)
                    try:
                        connection.request('GET',path)
                        response = connection.getresponse(); body = response.read()
                        assert response.status == expected, (path,response.status,body[:300])
                        return json.loads(body)
                    finally: connection.close()
                def metrics(): return request('/stream/metrics')['pipeline']['illumination']
                def wait(predicate):
                    deadline = time.monotonic()+15
                    while time.monotonic() < deadline:
                        m = metrics()
                        if predicate(m): return m
                        assert process.poll() is None, log_path.read_text()
                        time.sleep(.03)
                    raise AssertionError(m)
                if not experimental:
                    request('/dynamic/setting/off')
                    m = wait(lambda m: m['selected_policy']=='fixed')
                    assert m['experimental_available'] is available
                    request('/dynamic/setting/on')
                    wait(lambda m: m['selected_policy']=='legacy_dynamic')
                    request('/dynamic/setting/off')
                    request('/depth/setting/ir')
                    request('/depth/setting/depth')
                    request('/generalwrite/setting/4011/000c')
                    return
                m = wait(lambda m: m['acknowledged']==[1,1,1,1] and not m['pending'])
                assert m['detector']=='raw_depth_lt_250_proxy_including_zero'
                assert all(s['known'] and s['saturated']==s['eligible'] for s in m['sectors'])
                assert m['write_failures']==0 and m['failed_probes']==0
                assert m['write_successes']==(m['frame_id']+2)//3
                sectors = m['sectors']; ceilings = m['ceilings']
                request('/depth/setting/ir',409)
                request('/dynamic/setting/on',409)
                request('/dynamic/setting/off',409)
                request('/generalwrite/setting/4011/000c',409)
                request('/generalwrite/setting/c001/000c',409)
                for mode in (0,1,3,4,5,6):
                    previous = request('/frame/camera')['configuration_revision']
                    request(f'/colormode/setting/{mode}')
                    m = wait(lambda m: m['configuration_revision']>previous and m['detector_supported'])
                    assert m['sectors']==sectors and m['ceilings']==ceilings
                previous = request('/frame/camera')['configuration_revision']
                request('/smoothing/setting/on')
                m = wait(lambda m: m['configuration_revision']>previous and m['detector_supported'])
                assert m['sectors']==sectors
                request('/disable/camera')
                assert metrics()['acknowledged'] is None
                request('/enable/camera')
                m = wait(lambda m: m['acknowledged']==[1,1,1,1])
                assert m['ceilings']==ceilings
            finally:
                if process.poll() is None: process.send_signal(signal.SIGTERM)
                process.wait(timeout=20)
    run(False)
    if available: run(True)
print('Simulator illumination selection/current-frame/mode/ownership/cadence assertions passed')
