#!/usr/bin/env python3
"""SDK-free bundle/parser and explicitly verified simulator integration tests."""
import argparse
from array import array
import hashlib
import http.client
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('capture_tool', ROOT / 'tools/pointcloud_capture.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)
BINARY = None


def fixture(root, empty=False, large=False):
    root.mkdir()
    order = list(range(0, 640*480, 4)) if large else [] if empty else [0, 5, 9, 639, 643]
    raw = array('H', [1000]) * (640 * 480)
    raw[5] = 800
    raw[639] = 1200
    if sys.byteorder != 'little':
        raw.byteswap()
    raw_bytes = raw.tobytes()
    depths = {i: struct.unpack_from('<H', raw_bytes, i * 2)[0] for i in order}
    rays = [(0, 0, 1)] * len(order) if large else [(0, 0, 1), (0, 0, 1), (1, 0, 0), (-1, 0, 0), (0, 1, 0)]
    files = {'sensor-depth.u16le': raw.tobytes(), 'processed-depth.u16le': raw.tobytes(),
             'source-index.u32le': b''.join(struct.pack('<I', i) for i in order),
             'record-model.f32le': b''.join(struct.pack('<fffff', 1, 0, *ray) for ray in rays[:len(order)]),
             'cloud.i16le': b''.join(struct.pack('<hhhh', *(int(depths[i] * f) for f in ray), depths[i]) for i, ray in zip(order, rays)),
             'cleanup-flags.u8': bytes(len(order)),
             'runlines.json': json.dumps([[0, 640*480]] if large else [] if empty else [[0, 4], [5, 12], [639, 644]]).encode()}
    manifest = {'schema': 1, 'complete': True, 'width': 640, 'height': 480,
                'byte_order': 'little', 'model_layout': 'gain,offset,x,y,z:float32', 'image_origin': 'top-left',
                'record_count': len(order), 'smoothed': 0, 'plugin': 0, 'sampling': 0,
                'calibrated': 1, 'acquisition_backend': 'synthetic-fixture', 'frame_id': 1, 'revision': 1, 'captured_at_ms': 1,
                'process_started_ns': 1, 'process_id': 1, 'processed_units': 'mm',
                'units_scope': 'calibration_aoi', 'binary_sha256': '0' * 64,
                'sizes': {k: len(v) for k, v in files.items()}}
    files['capture.json'] = json.dumps(manifest).encode()
    for name, data in files.items():
        (root / name).write_bytes(data)
    checksums(root)
    return root


def checksums(root):
    (root / 'SHA256SUMS').write_text(''.join(hashlib.sha256((root / name).read_bytes()).hexdigest() + '  ' + name + '\n' for name in sorted(tool.FILES_V2 if (root / 'pixel-status.u8').exists() else tool.FILES)))


class ParserTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='chiton-recorded-bundle-')
        self.root = fixture(Path(self.temp.name) / 'capture')

    def tearDown(self):
        self.temp.cleanup()

    def test_numerical_correspondence_csv(self):
        b = tool.load_bundle(self.root)
        self.assertEqual(b['indices'], [0, 5, 9, 639, 643])
        self.assertEqual(b['report']['projection_mismatch_records'], [])
        self.assertEqual(b['report']['processing_mismatch_records'], [])
        self.assertIn(b'4,643,3,1,1000,1000', tool.selected_csv(b, [4]))

    def test_empty(self):
        root = fixture(Path(self.temp.name) / 'empty', empty=True)
        self.assertEqual(tool.load_bundle(root)['indices'], [])
        if not shutil.which('node') or not Path('/usr/bin/chromium').exists():
            self.skipTest('Node/Chromium unavailable: empty capture image export unverified')
        output = Path(self.temp.name) / 'empty-output'
        output.mkdir()
        server = tool.serve(tool.load_bundle(root), output)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            subprocess.run(['node', str(ROOT / 'tests/pointcloud_capture_browser.js'),
                            f'http://127.0.0.1:{server.server_port}', '--empty'], cwd=ROOT, check=True, timeout=80)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
        self.assertEqual(json.loads((output / 'export_0001/selection.json').read_text())['record_ids'], [])

    def test_incomplete(self):
        (self.root / 'INCOMPLETE').touch()
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            tool.load_bundle(self.root)

    def test_hash_rejection(self):
        with (self.root / 'cloud.i16le').open('ab') as stream:
            stream.write(b'X')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            tool.load_bundle(self.root)

    def test_order_rejection(self):
        (self.root / 'source-index.u32le').write_bytes(struct.pack('<5I', 0, 5, 9, 639, 644))
        checksums(self.root)
        with self.assertRaisesRegex(ValueError, 'index/order'):
            tool.load_bundle(self.root)

    def test_depth_rejection(self):
        b = bytearray((self.root / 'cloud.i16le').read_bytes())
        struct.pack_into('<h', b, 6, 999)
        (self.root / 'cloud.i16le').write_bytes(b)
        checksums(self.root)
        with self.assertRaisesRegex(ValueError, 'D/source'):
            tool.load_bundle(self.root)

    def test_missing_coefficients(self):
        (self.root / 'record-model.f32le').write_bytes(b'')
        checksums(self.root)
        with self.assertRaisesRegex(ValueError, 'length'):
            tool.load_bundle(self.root)

    def test_symlink_rejection(self):
        path = self.root / 'sensor-depth.u16le'
        path.rename(self.root / 'other')
        path.symlink_to('other')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            tool.load_bundle(self.root)

    def test_nonfinite_model(self):
        b = bytearray((self.root / 'record-model.f32le').read_bytes())
        struct.pack_into('<f', b, 0, float('nan'))
        (self.root / 'record-model.f32le').write_bytes(b)
        checksums(self.root)
        with self.assertRaisesRegex(ValueError, 'nonfinite'):
            tool.load_bundle(self.root)

    def test_wrong_projection_preserved_as_evidence(self):
        b = bytearray((self.root / 'cloud.i16le').read_bytes())
        struct.pack_into('<h', b, 0, 42)
        (self.root / 'cloud.i16le').write_bytes(b)
        checksums(self.root)
        self.assertEqual(tool.load_bundle(self.root)['report']['projection_mismatch_records'], [0])

    def test_filtered_v2_mapping(self):
        original = tool.load_bundle(self.root)
        statuses = bytearray(640*480)
        for i, member in enumerate(original['domain']):
            statuses[i] = 0 if member else 1
        statuses[0] = 2
        raw = bytearray((self.root / 'sensor-depth.u16le').read_bytes())
        struct.pack_into('<H', raw, 0, 0)
        (self.root / 'sensor-depth.u16le').write_bytes(raw)
        for name, width in [('source-index.u32le', 4), ('record-model.f32le', 20), ('cloud.i16le', 8), ('cleanup-flags.u8', 1)]:
            (self.root / name).write_bytes((self.root / name).read_bytes()[width:])
        (self.root / 'pixel-status.u8').write_bytes(statuses)
        m = original['manifest']
        m['schema'] = 2
        m['record_count'] = 4
        m['filter'] = {'policy': 'runlines_valid_values_v1', 'candidate_count': 5, 'excluded_sample_count': 1,
                       'status_counts': [statuses.count(s) for s in range(8)]}
        for name in tool.FILES_V2 - {'capture.json'}:
            m['sizes'][name] = (self.root / name).stat().st_size
        (self.root / 'capture.json').write_text(json.dumps(m))
        checksums(self.root)
        b = tool.load_bundle(self.root)
        self.assertEqual(b['indices'], [5, 9, 639, 643])
        self.assertEqual(b['report']['excluded_sample_count'], 1)
        self.assertEqual(b['pixel_status'][0], 2)
        statuses[5] = 4  # would shift record correspondence: reject, never reindex silently
        (self.root / 'pixel-status.u8').write_bytes(statuses)
        checksums(self.root)
        with self.assertRaisesRegex(ValueError, 'counts|order'):
            tool.load_bundle(self.root)

    def test_invalid_png(self):
        with self.assertRaises(ValueError):
            tool.validate_png(b'not PNG')

    def test_browser_picking_export(self):
        if not shutil.which('node') or not Path('/usr/bin/chromium').exists():
            self.skipTest('Node/Chromium unavailable: recorded picking/export unverified')
        output = Path(self.temp.name) / 'output'
        output.mkdir()
        server = tool.serve(tool.load_bundle(self.root), output)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            subprocess.run(['node', str(ROOT / 'tests/pointcloud_capture_browser.js'), f'http://127.0.0.1:{server.server_port}'], cwd=ROOT, check=True, timeout=80)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()
        exported = output / 'export_0001'
        self.assertFalse((exported / 'INCOMPLETE').exists())
        selected = json.loads((exported / 'selection.json').read_text())
        self.assertEqual(selected['record_ids'], [0])
        for name in tool.IMAGES:
            tool.validate_png((exported / name).read_bytes())
        self.assertIn(b'0,0,0,0,1000,1000', (exported / 'selected-pixels.csv').read_bytes())
        manifest = json.loads((exported / 'render-manifest.json').read_text())
        for name, expected in manifest['sha256'].items():
            self.assertEqual(hashlib.sha256((exported / name).read_bytes()).hexdigest(), expected)

    def test_loopback_allowlist(self):
        output = Path(self.temp.name) / 'secure-output'
        output.mkdir()
        server = tool.serve(tool.load_bundle(self.root), output)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            for method, path, headers, expected in [
                    ('GET', '/raw/camera', {}, 404), ('GET', '/../capture.json', {}, 404),
                    ('GET', '/', {'Host': 'device.example'}, 403),
                    ('POST', '/export', {}, 403),
                    ('POST', '/export', {'Origin': 'http://device.example', 'X-Capture-Token': 'wrong'}, 403)]:
                connection = http.client.HTTPConnection('127.0.0.1', server.server_port, timeout=5)
                connection.request(method, path, headers=headers)
                response = connection.getresponse()
                self.assertEqual(response.status, expected)
                response.read()
                connection.close()
            self.assertEqual(list(output.iterdir()), [])
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_large_record_id(self):
        if not shutil.which('node') or not Path('/usr/bin/chromium').exists():
            self.skipTest('Node/Chromium unavailable: large record-ID picking unverified')
        root = fixture(Path(self.temp.name) / 'large', large=True)
        output = Path(self.temp.name) / 'large-output'
        output.mkdir()
        server = tool.serve(tool.load_bundle(root), output)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            subprocess.run(['node', str(ROOT / 'tests/pointcloud_capture_browser.js'),
                            f'http://127.0.0.1:{server.server_port}', '--large'], cwd=ROOT, check=True, timeout=80)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_native_one_shot(self):
        if BINARY is None:
            self.skipTest('requires --binary explicitly verified SDK-free simulator')
        from runtime_support import runtime_directory, simulator_environment
        cache = BINARY.parent / 'CMakeCache.txt'
        self.assertRegex(cache.read_text(), r'(?m)^USE_ADITOF_STUB:BOOL=ON$')
        self.assertIn(b'file-backed camera: hardware temperature monitor disabled', BINARY.read_bytes())
        with runtime_directory('chiton-capture-native-') as directory:
            root = Path(directory)
            shutil.copyfile(ROOT / 'release/chiton.json', root / 'chiton.json')
            depth = struct.pack('<H', 800) * (640 * 480)
            (root / 'depth.bin').write_bytes(depth)
            (root / 'ir.bin').write_bytes(depth)
            env = simulator_environment(root)
            env.update(CHITON_RESOURCE_DIR=str(root), CHITON_CONFIG_FILE=str(root / 'chiton.json'), CHITON_DISPLAY='headless', CHITON_CALIBRATION_FILE=str(root / 'absent.bin'), ADITOF_STUB_DEPTH=str(root / 'depth.bin'), ADITOF_STUB_IR=str(root / 'ir.bin'))
            def run(name, *extra):
                p = subprocess.run([str(BINARY), '--diagnostic-capture-dir', str(root / name), '--diagnostic-settle-frames', '0', *extra], cwd=root, env=env, capture_output=True, timeout=35)
                (root / (name + '.log')).write_bytes(p.stdout + p.stderr)
                return p
            p = run('capture')
            self.assertEqual(p.returncode, 0, p.stderr[-3000:])
            self.assertNotIn(b'web server listening', p.stdout + p.stderr)
            b = tool.load_bundle(root / 'capture')
            self.assertEqual((root / 'capture/sensor-depth.u16le').read_bytes(), depth)
            self.assertEqual(b['manifest']['acquisition_backend'], 'file-backed-stub')
            self.assertEqual(b['report']['projection_mismatch_records'], [])
            self.assertEqual(b['report']['processing_mismatch_records'], [])
            self.assertEqual(b['manifest']['binary_sha256'], hashlib.sha256(BINARY.read_bytes()).hexdigest())
            aggressive_run = run('aggressive', '--sampled-boundary-cleanup', 'aggressive-mm-v1')
            self.assertEqual(aggressive_run.returncode, 0, aggressive_run.stderr[-3000:])
            aggressive = tool.load_bundle(root / 'aggressive')
            self.assertEqual(aggressive['manifest']['cleanup']['algorithm'], 3)
            self.assertEqual(aggressive['indices'], b['indices'])
            self.assertEqual(aggressive['records'], b['records'])
            for filename in ('sensor-depth.u16le', 'processed-depth.u16le', 'pixel-status.u8', 'record-model.f32le'):
                self.assertEqual((root / 'aggressive' / filename).read_bytes(), (root / 'capture' / filename).read_bytes())
            original = (root / 'capture/capture.json').read_bytes()
            self.assertNotEqual(run('capture').returncode, 0)
            self.assertEqual((root / 'capture/capture.json').read_bytes(), original)
            # A plugin is refused before dlopen or SDK work.
            (root / 'api.so').write_bytes(b'not executable')
            self.assertNotEqual(run('plugin').returncode, 0)
            self.assertTrue((root / 'plugin/INCOMPLETE').exists())
            (root / 'api.so').unlink()
            # No-return and out-of-range pixels are exclusions, not capture failures.
            (root / 'depth.bin').write_bytes(struct.pack('<H', 65535) * (640 * 480))
            p = run('unsafe')
            self.assertEqual(p.returncode, 0, p.stderr[-2000:])
            filtered = tool.load_bundle(root / 'unsafe')
            self.assertGreater(filtered['report']['excluded_sample_count'], 0)
            self.assertFalse((root / 'unsafe/INCOMPLETE').exists())
            (root / 'depth.bin').write_bytes(bytes(614400))
            p = run('zero', '--sampled-boundary-cleanup', 'aggressive-mm-v1')
            self.assertEqual(p.returncode, 0, p.stderr[-2000:])
            zero = tool.load_bundle(root / 'zero')
            self.assertEqual(zero['manifest']['record_count'], 0)
            self.assertEqual(zero['manifest']['filter']['candidate_count'], zero['manifest']['filter']['excluded_sample_count'])
            self.assertGreater(zero['report']['status_counts']['zero_sensor'], 0)
            self.assertEqual((root / 'zero/sensor-depth.u16le').stat().st_size, 614400)
            mixed_bytes = b''.join(struct.pack('<H', 0 if i%3==0 else 800) for i in range(640*480))
            (root / 'depth.bin').write_bytes(mixed_bytes)
            p = run('mixed', '--sampled-boundary-cleanup', 'aggressive-mm-v1')
            self.assertEqual(p.returncode, 0, p.stderr[-2000:])
            mixed = tool.load_bundle(root / 'mixed')
            self.assertEqual(mixed['indices'], [i for i in b['indices'] if i%3])
            by_index = dict(zip(b['indices'], b['records']))
            for i, record in zip(mixed['indices'], mixed['records']):
                self.assertEqual(record, by_index[i])
            self.assertGreater(mixed['manifest']['record_count'], 0)
            self.assertGreater(mixed['report']['excluded_sample_count'], 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path)
    args, rest = parser.parse_known_args()
    if args.binary:
        BINARY = args.binary.resolve()
    unittest.main(argv=[sys.argv[0], *rest])
