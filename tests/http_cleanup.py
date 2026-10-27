#!/usr/bin/env python3
"""Actual file-backed server only: cleanup tuning, coherent wire, legacy parity."""
import http.client
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import struct
import subprocess
import sys
import time
from urllib.parse import urlencode
from runtime_support import runtime_directory, simulator_environment

ROOT = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
assert re.search(r'^USE_ADITOF_STUB:BOOL=ON$', (binary.parent/'CMakeCache.txt').read_text(), re.M), 'Refusing hardware build'
assert b'file-backed camera: hardware temperature monitor disabled' in binary.read_bytes()
out = ROOT/'_build/sampled-boundary-cleanup/integration/validation'
out.mkdir(parents=True, exist_ok=True)
with runtime_directory('chiton-cleanup-') as directory:
    root = Path(directory)
    shutil.copyfile(ROOT/'release/chiton.json',root/'chiton.json')
    frame = bytearray(640*480*2)
    for y in range(480):
        for x in range(640):
            struct.pack_into('<H',frame,2*(y*640+x),1000 if x%8<4 else 2000)
    (root/'depth.bin').write_bytes(frame)
    env = simulator_environment(directory)
    env.update(CHITON_RESOURCE_DIR=str(root), CHITON_ASSET_DIR=str(ROOT/'release/files'),
               ADITOF_STUB_DEPTH=str(root/'depth.bin'), ADITOF_STUB_IR=str(root/'depth.bin'),
               CHITON_CALIBRATION_FILE=str(root/'calibration.bin'))
    with (out/'cleanup-runtime.log').open('w') as log:
        process = subprocess.Popen([str(binary),'--display','headless'],cwd=root,env=env,stdout=log,stderr=log)
        try:
            deadline = time.monotonic()+20
            match = None
            while time.monotonic()<deadline and process.poll() is None:
                text = (out/'cleanup-runtime.log').read_text()
                match = re.search(r'web server listening on port (\d+)',text)
                if match: break
                time.sleep(.05)
            assert match and process.poll() is None, text
            assert 'file-backed camera: hardware temperature monitor disabled' in text
            assert 'process_standard_frame callback found' not in text
            port = int(match[1])
            def request(path, expected=200, method='GET'):
                c = http.client.HTTPConnection('127.0.0.1',port,timeout=5)
                try:
                    c.request(method,path)
                    r = c.getresponse(); body = r.read(); headers = dict((k.lower(),v) for k,v in r.getheaders())
                    assert r.status in (expected if isinstance(expected,tuple) else (expected,)),(path,r.status,body[:250])
                    headers[':status']=r.status
                    assert len(body)==int(headers['content-length'])
                    return body,headers
                finally: c.close()
            def config(): return json.loads(request('/api/cleanup/v1/config')[0])
            def update(c, generation, expected=200, extra=''):
                return request('/api/cleanup/v1/config?'+urlencode({**c,'expected_generation':generation})+extra,expected,'POST')
            def cloud(generation=None):
                deadline = time.monotonic()+5
                while True:
                    b,h = request('/api/cleanup/v1/cloud',(200,503))
                    if h[':status']==503:
                        assert time.monotonic()<deadline, b
                        time.sleep(.02); continue
                    assert b[:8]==b'CHCLD001' and len(b)>=128
                    header,n,geometry,flags = struct.unpack_from('<IIII',b,8)
                    assert header==128 and geometry==n*8 and flags==n and len(b)==128+n*9
                    identity,revision,captured,gen = struct.unpack_from('<QQQQ',b,24)
                    assert identity and captured and n<=230400
                    if generation is None or gen==generation:
                        return {'bytes':b,'n':n,'frame':identity,'revision':revision,'generation':gen,'status':struct.unpack_from('<I',b,60)[0],
                                'records':b[128:128+n*8],'flags':b[128+n*8:]}
                    assert time.monotonic()<deadline, 'configuration never applied to publication'
                    time.sleep(.02)
            for route in ['/depth/setting/depth','/smoothing/setting/off','/chitoncal/setting/on','/colormode/setting/4']:
                request(route)
            s = config(); assert s['generation']==1 and s['config']['enabled']==0
            assert s['constraints']['algorithm']==[1,2,3]
            original = cloud(1); assert original['status']==1 and not any(original['flags'])
            enabled = {**s['config'],'enabled':1,'algorithm':3}
            ack = json.loads(update(enabled,1)[0]); assert ack['generation']==2 and ack['config']==enabled
            current = cloud(2); assert current['status']==2, current
            assert current['records']==original['records'] and current['revision']==original['revision']
            assert set(current['flags'])<={1,2,4,8}
            subprocess.run(['node','-e', "const b=require('fs').readFileSync(0),C=require('./release/files/console-cloud');const m=C.decodeCleanup(b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength));if(m.cleanup.status!==2||m.cleanup.generation!==2)throw new Error('native wire decode');console.log('http_cleanup: actual C++ ready-frame decoded by console JavaScript');"],input=current['bytes'],cwd=ROOT,check=True)
            for bad in [{'boundary_gap':65536},{'boundary_gap':-1},{'boundary_gap':1.5},{'surface_support':256},{'center_support':256},{'algorithm':0},{'algorithm':4},{'evidence_max':65536}]:
                update({**enabled,**bad},2,400); assert config()['generation']==2
            update(enabled,1,409)
            update(enabled,2,400,'&enabled=1')
            update(enabled,2,400,'&unknown=1')
            assert config()['generation']==2
            # Frozen publication proves legacy bytes and envelope have ONE identity.
            Path(env['ADITOF_STUB_PAUSE_FILE']).touch(); time.sleep(.15)
            frozen = cloud(2)
            legacy, headers = request('/pointcloud/camera')
            assert int(headers['x-chiton-frame-id'])==frozen['frame'] and legacy==frozen['records']
            raw_before = request('/raw/camera')[0]
            disabled = {**enabled,'enabled':0}
            ack = json.loads(update(disabled,2)[0]); assert ack['generation']==3
            pending = cloud(); assert pending['generation']==2
            assert request('/raw/camera')[0]==raw_before
            Path(env['ADITOF_STUB_PAUSE_FILE']).unlink()
            off = cloud(3); assert off['status']==1 and not any(off['flags']) and off['records']==frozen['records']
            # Slow/disconnected network reader must not hold camera ownership.
            stalled = socket.socket(); stalled.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1024); stalled.connect(('127.0.0.1',port))
            stalled.sendall(b'GET /api/cleanup/v1/cloud HTTP/1.1\r\nHost: test\r\n\r\n')
            time.sleep(.3); stalled.close()
            after = cloud(3); assert after['frame']>off['frame']+2
            request('/disable/camera'); request('/api/cleanup/v1/cloud',503); assert config()['generation']==3
            request('/enable/camera'); cloud(3)
            # No tuning heuristics or coupled limits: values round-trip through
            # the actual C++ API and same-frame wire header, including extremes.
            generation = 3
            variants = [{'algorithm':1}, {'algorithm':2}, {'algorithm':3}, {'boundary_gap':80}, {'center_margin':40}, {'surface_support':0},
                        {'center_support':255}, {'evidence_min':65535,'evidence_max':0},
                        {'ramp_residual':65535}, {'anchor_tolerance':0},
                        {k:0 for k in enabled if k not in ('enabled','algorithm')},
                        {k:255 if k.endswith('_support') else 65535 for k in enabled if k not in ('enabled','algorithm')}]
            for variant in variants:
                tuning = {**enabled, **variant}
                ack = json.loads(update(tuning,generation)[0]); generation += 1
                assert ack['generation']==generation and ack['config']==tuning
                assert config()['config']==tuning
                published = cloud(generation)
                assert published['status']==2 and published['records']==original['records']
                assert struct.unpack_from('<I',published['bytes'],56)[0]==tuning['algorithm']
                if tuning['algorithm']==3: assert struct.unpack_from('<I',published['bytes'],112)[0]==0
                assert struct.unpack_from('<6H',published['bytes'],72)==tuple(tuning[k] for k in
                    ('evidence_min','evidence_max','boundary_gap','anchor_tolerance','center_margin','ramp_residual'))
                assert published['bytes'][84:86]==bytes((tuning['surface_support'],tuning['center_support']))
            request('/colormode/setting/1'); request('/api/cleanup/v1/cloud',409)
            request('/api/cleanup/v1/cloud?after=0&after=1',400)
            print('http_cleanup: real server atomic tuning/CAS, applied generations, same-frame flags+XYZD, legacy/raw parity, slow reader capture progress, disabled/mode gates passed')
        finally:
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: process.kill(); process.wait()
