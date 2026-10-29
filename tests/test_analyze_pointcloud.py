#!/usr/bin/env python3
"""Offline analyzer contracts: no server, hardware, or third-party packages."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('analyzer', ROOT / 'tools/analyze_pointcloud.py')
a = importlib.util.module_from_spec(spec)
spec.loader.exec_module(a)


def headers(n=2, order='little'):
    return {'Content-Type': 'application/octet-stream',
        **{'X-Chiton-'+k: v for k, v in {
            'Layout': 'int16_xyzd', 'Color-Mode': '4', 'Frame-Type': 'depth',
            'Units': 'depth_sensor_units', 'Source-Units': 'depth_sensor_units',
            'Units-Scope': 'full_sensor', 'Invalid-Sample-Policy': 'all_uint16_included',
            'Byte-Order': order, 'Width': str(n), 'Height': '1', 'Frame-Id': '7',
            'Configuration-Revision': '0'}.items()}}


class AnalyzerTests(unittest.TestCase):
    def test_obj_export_recovery(self):
        self.assertEqual(a.parse_obj(b'v -0.0002 0.0000 0.1333\n', True), [(-1, 0, 800)])
        self.assertAlmostEqual(a.parse_obj(b'v 0.0001 0 0\n')[0][0], .6)

    def test_obj_empty_comments(self):
        self.assertEqual(a.parse_obj(b'# comment\n\n'), [])
        self.assertEqual(a.parse_obj(b'v 0 0 0 # origin\n'), [(0, 0, 0)])

    def test_obj_reject_malformed(self):
        for body in (b'f 1 2 3', b'v nan 0 0', b'v inf 0 0', b'v 1 2', b'v 0 0 0 1',
                     b'v 1e308 0 0', b'v 6 0 0', b'\xff', b'#' + b'a'*1024):
            with self.subTest(body=body[:20]), self.assertRaises((ValueError, UnicodeError)):
                a.parse_obj(body)

    def test_obj_lattice(self):
        with self.assertRaises(ValueError):
            a.parse_obj(b'v .00008 0 0', True)

    def test_obj_caps(self):
        with self.assertRaises(ValueError):
            a.parse_obj(b'v 0 0 0\n' * (a.MAX_RECORDS+1))
        with self.assertRaises(ValueError):
            a.parse_obj(b' ' * (a.MAX_OBJ+1))

    def test_endian_and_signed_first(self):
        rows = [(-32768, 0, 32767, -1), (0, -10, 0, 100)]
        for order, fmt in [('little', '<'), ('big', '>')]:
            body = b''.join(struct.pack(fmt+'hhhh', *r) for r in rows)
            self.assertEqual(a.parse_binary(body, headers(order=order)), rows)

    def test_binary_empty(self):
        self.assertEqual(a.parse_binary(b'', headers(0)), [])

    def test_binary_max(self):
        rows = a.parse_binary(b'\0' * a.MAX_BINARY, headers(a.MAX_RECORDS))
        self.assertEqual(len(rows), a.MAX_RECORDS)

    def test_binary_caps_alignment_count(self):
        for body, h in [(b'\0'*7, headers(1)), (b'\0'*8, headers(2)),
                        (b'\0'*(a.MAX_BINARY+8), headers(a.MAX_RECORDS+1))]:
            with self.assertRaises(ValueError):
                a.parse_binary(body, h)

    def test_binary_required_metadata(self):
        for key in headers():
            h = headers(); del h[key]
            with self.subTest(key=key), self.assertRaises(ValueError):
                a.parse_binary(b'\0'*16, h)

    def test_binary_bad_metadata(self):
        for key, value in [('Width', '-1'), ('Width', str(2**65)), ('Height', '2'),
                ('Frame-Id', '0'), ('Frame-Id', '1.0'), ('Frame-Id', '١'),
                ('Configuration-Revision', '-1'), ('Color-Mode', '3'), ('Layout', 'uint16_xyzd'),
                ('Byte-Order', 'native'), ('Frame-Type', 'ir'), ('Units', 'meters'),
                ('Source-Units', 'mm'), ('Units-Scope', 'calibration_aoi'),
                ('Invalid-Sample-Policy', 'filtered'), ('Frame-Age-Ms', '-1')]:
            h = headers(); h['X-Chiton-'+key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                a.parse_binary(b'\0'*16, h)
        h = headers(); h['Content-Length'] = '8'
        with self.assertRaises(ValueError):
            a.parse_binary(b'\0'*16, h)

    def test_calibrated_units(self):
        h = headers(1)
        h.update({'X-Chiton-Units': 'mm', 'X-Chiton-Source-Units': 'mm', 'X-Chiton-Units-Scope': 'calibration_aoi'})
        self.assertEqual(a.parse_binary(b'\0'*8, h), [(0, 0, 0, 0)])

    def test_statistics_and_angles(self):
        r = a.analyze([(0,0,0), (-3,0,4), (0,1,0), (0,-2,0)])
        self.assertEqual(r['origin_count'], 1)
        self.assertEqual(r['angular_excluded_origin_count'], 1)
        self.assertEqual(r['zero_y_count'], 2)
        self.assertEqual(r['zero_y_nonorigin_count'], 1)
        self.assertEqual(r['near_y_count'], 3)
        self.assertEqual(r['radius']['max'], 5)
        self.assertIsNone(r['depth'])
        self.assertEqual(sum(r['xyz']['X']['histogram']), 4)

    def test_signed_depth_residual(self):
        r = a.analyze([(3,0,4,5), (0,0,0,-1), (0,0,0,0)])
        self.assertEqual(r['negative_depth_count'], 1)
        self.assertEqual(r['norm_minus_positive_signed_depth']['max'], 0)
        self.assertEqual(r['norm_minus_positive_signed_depth']['count'], 1)

    def test_direction_bins(self):
        r = a.analyze([(0,0,1), (0,0,100), (0,0,0)])
        for resolution in r['direction_bins']:
            self.assertEqual(resolution['occupied'], 1)
            self.assertEqual(resolution['largest_radial_spans'][0]['span'], 99)

    def test_empty_and_degenerate(self):
        self.assertEqual(a.analyze([])['radius']['count'], 0)
        for points in ([], [(0,0,0)], [(1,1,1)]*4):
            svg = a.scatter_svg(points)
            self.assertNotIn('nan', svg.lower())
            self.assertNotIn('inf', svg.lower())
            self.assertIn('No connections/mesh', svg)
            self.assertIn('<svg', a.histogram_svg(a.analyze(points)))

    def test_cli_evidence_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp); source = base/'cloud.obj'; out = base/'case'
            body = b'v 0.1333 0 0\n'; source.write_bytes(body)
            command = [sys.executable, str(ROOT/'tools/analyze_pointcloud.py'), str(source),
                       '--out', str(out), '--recover-export-integers']
            p = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stderr)
            manifest = json.loads((out/'manifest.json').read_text())
            self.assertEqual(manifest['sha256'], hashlib.sha256(body).hexdigest())
            self.assertEqual((out/'original.obj').read_bytes(), body)
            self.assertEqual(json.loads((out/'summary.json').read_text())['first_record'], [800,0,0])
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(source.read_bytes(), body)

    def test_binary_cli_headers_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp); source = base/'cloud.bin'; hp=base/'headers.json'; out=base/'case'
            source.write_bytes(struct.pack('>hhhh', 0,-1,3,4)); hp.write_text(json.dumps(headers(1, 'big')))
            p = subprocess.run([sys.executable, str(ROOT/'tools/analyze_pointcloud.py'), str(source),
                                '--headers', str(hp), '--out', str(out)], capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertEqual((out/'original-headers.json').read_bytes(), hp.read_bytes())
            self.assertTrue((out/'depth.svg').is_file())
            self.assertTrue((out/'distributions.svg').is_file())
            self.assertEqual(json.loads((out/'manifest.json').read_text())['headers_sha256'],
                             hashlib.sha256(hp.read_bytes()).hexdigest())

    def test_geometry_cli_empty_selection(self):
        executable = os.environ.get('CHITON_POINTCLOUD_GEOMETRY')
        if not executable:
            self.skipTest('set CHITON_POINTCLOUD_GEOMETRY to exercise the compiled geometry CLI')
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp); config = base/'empty.json'; sidecar = base/'empty.csv'
            config.write_text(json.dumps(dict(center_x=320, center_y=240, inner_dist=0, outer_dist=0)))
            p = subprocess.run([executable, '--config', str(config), '--sidecar', str(sidecar)],
                               capture_output=True, text=True)
            self.assertEqual(p.returncode, 2, p.stderr)
            self.assertIn('geometry selects no emitted records', p.stderr)
            self.assertFalse(sidecar.exists())
            self.assertFalse(Path(str(sidecar)+'.svg').exists())
            self.assertFalse(Path(str(sidecar)+'.json').exists())

    def test_cli_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            base=Path(tmp); source=base/'cloud.bin'; source.write_bytes(b'\0'*8)
            for extra in ([], ['--near-y', 'nan']):
                out=base/'case'
                p=subprocess.run([sys.executable,str(ROOT/'tools/analyze_pointcloud.py'),str(source),
                                  '--out',str(out),*extra],capture_output=True)
                self.assertNotEqual(p.returncode,0)
                self.assertFalse(out.exists())


if __name__ == '__main__':
    unittest.main()
