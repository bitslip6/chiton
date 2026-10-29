#!/usr/bin/env python3
"""Validate and inspect a local same-frame diagnostic bundle. Never contacts a camera."""
import argparse
import base64
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import secrets
import struct
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[1]
FILES = {'sensor-depth.u16le', 'processed-depth.u16le', 'cloud.i16le',
         'source-index.u32le', 'record-model.f32le', 'cleanup-flags.u8', 'runlines.json', 'capture.json'}
FILES_V2 = FILES | {'pixel-status.u8'}
STATUS_NAMES = ['valid', 'outside_runlines', 'zero_sensor', 'calibration_nonfinite',
                'calibration_range', 'zero_processed', 'projection_nonfinite', 'projection_range']
IMAGES = {'cloud-top.png', 'cloud-front.png', 'cloud-view.png', 'sensor-depth.png',
          'sensor-depth-marked.png', 'processed-depth.png', 'processed-depth-marked.png', 'comparison.png'}
LIMIT = 230400
ASSETS = {'three.min.js', 'console-model.js', 'console-cloud.js',
          'console-cloud-material.js', 'console-cloud-view.js'}


def read_local(root, name, limit):
    p = root / name
    if p.is_symlink() or not p.is_file() or p.stat().st_size > limit:
        raise ValueError('missing, symlink or oversized file: ' + name)
    with p.open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('file grew beyond cap: ' + name)
    return data


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def load_bundle(directory):
    root = Path(directory).resolve()
    if (root / 'INCOMPLETE').exists() or (root / 'capture.json.tmp').exists():
        raise ValueError('incomplete capture; no exact correspondence may be claimed')
    hashes = {}
    for line in read_local(root, 'SHA256SUMS', 4096).decode('ascii').splitlines():
        digest, name = line.split('  ', 1)
        if name not in FILES_V2 or name in hashes or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError('invalid checksum manifest')
        hashes[name] = digest
    if set(hashes) not in (FILES, FILES_V2):
        raise ValueError('incomplete checksum set')
    contents = {}
    for name in hashes:
        data = read_local(root, name, 5 * 1024 * 1024)
        if hashlib.sha256(data).hexdigest() != hashes[name]:
            raise ValueError('checksum mismatch: ' + name)
        contents[name] = data
    m = json.loads(contents['capture.json'])
    def integer(value, lo, hi):
        if type(value) is not int or not lo <= value <= hi:
            raise ValueError('invalid integer metadata')
        return value
    n = integer(m['record_count'], 0, LIMIT)
    if (m['schema'] not in (1, 2) or m['complete'] is not True or m['width'] != 640 or m['height'] != 480 or
        m['byte_order'] != 'little' or m['model_layout'] != 'gain,offset,x,y,z:float32' or m['image_origin'] != 'top-left'):
        raise ValueError('unsupported capture schema/layout')
    if set(hashes) != (FILES_V2 if m['schema'] == 2 else FILES):
        raise ValueError('schema/checksum set mismatch')
    for key in ('smoothed', 'plugin', 'sampling'):
        if m[key] != 0:
            raise ValueError('unsupported processing context: ' + key)
    integer(m['calibrated'], 0, 1)
    if m['acquisition_backend'] not in ('hardware-sdk', 'file-backed-stub', 'synthetic-fixture'):
        raise ValueError('missing/unknown acquisition backend')
    for key in ('frame_id', 'revision', 'captured_at_ms', 'process_started_ns'):
        integer(m[key], 1, 2**64 - 1)
    if m['processed_units'] != ('mm' if m['calibrated'] else 'depth_sensor_units') or m['units_scope'] != ('calibration_aoi' if m['calibrated'] else 'full_sensor'):
        raise ValueError('inconsistent units/scope')
    if len(m['binary_sha256']) != 64 or any(c not in '0123456789abcdef' for c in m['binary_sha256']):
        raise ValueError('missing binary provenance')
    expected = {'sensor-depth.u16le': 614400, 'processed-depth.u16le': 614400,
                'cloud.i16le': n * 8, 'source-index.u32le': n * 4,
                'record-model.f32le': n * 20, 'cleanup-flags.u8': n}
    for name, length in expected.items():
        if len(contents[name]) != length or m['sizes'][name] != length:
            raise ValueError('length/count mismatch: ' + name)
    runs = json.loads(contents['runlines.json'])
    if type(runs) is not list or len(runs) > 2048 or m['sizes']['runlines.json'] != len(contents['runlines.json']):
        raise ValueError('invalid runlines')
    order, domain = [], bytearray(640 * 480)
    end = 0
    for run in runs:
        if type(run) is not list or len(run) != 2:
            raise ValueError('invalid runline')
        begin, stop = (integer(v, 0, 640 * 480) for v in run)
        if begin < end or stop < begin:
            raise ValueError('overlapping/reversed runline')
        end = stop
        domain[begin:stop] = b'\1' * (stop - begin)
        order.extend(range(begin, stop & ~3, 4))
        if len(order) > LIMIT:
            raise ValueError('runline record cap')
    candidates = len(order)
    if m['schema'] == 2:
        status = list(contents['pixel-status.u8'])
        if len(status) != 640*480 or m['sizes']['pixel-status.u8'] != len(status) or any(s >= len(STATUS_NAMES) for s in status):
            raise ValueError('invalid pixel-status mask')
        if any((s == 1) != (not member) for s, member in zip(status, domain)):
            raise ValueError('pixel status/runline domain mismatch')
        order = [i for i in order if status[i] == 0]
        f = m['filter']
        if (f['policy'] != 'runlines_valid_values_v1' or f['candidate_count'] != candidates or
                f['excluded_sample_count'] != candidates - len(order) or
                f['status_counts'] != [status.count(s) for s in range(len(STATUS_NAMES))]):
            raise ValueError('exclusion counts/policy mismatch')
    else:
        status = [0 if member else 1 for member in domain]
    indices = [i[0] for i in struct.iter_unpack('<I', contents['source-index.u32le'])]
    if indices != order or len(indices) != n:
        raise ValueError('source index/order/count mismatch')
    raw = [v[0] for v in struct.iter_unpack('<H', contents['sensor-depth.u16le'])]
    processed = [v[0] for v in struct.iter_unpack('<H', contents['processed-depth.u16le'])]
    records = list(struct.iter_unpack('<hhhh', contents['cloud.i16le']))
    models = list(struct.iter_unpack('<fffff', contents['record-model.f32le']))
    flags = list(contents['cleanup-flags.u8'])
    if any(flag not in (0, 1, 2, 4, 8) for flag in flags):
        raise ValueError('invalid cleanup flag')
    projection_mismatch = []
    processing_mismatch = []
    zero_y_factors = []
    factor_norm_extrema = [math.inf, -math.inf]
    for k, (i, record, model) in enumerate(zip(indices, records, models)):
        if not all(math.isfinite(v) for v in model):
            raise ValueError('nonfinite captured model')
        if (m['schema'] == 2 and (raw[i] == 0 or processed[i] == 0)) or record[3] != processed[i] or processed[i] > 32767:
            raise ValueError('cloud D/source-frame mismatch')
        if model[3] == 0:
            zero_y_factors.append(k)
        norm = math.sqrt(sum(f * f for f in model[2:]))
        factor_norm_extrema = [min(factor_norm_extrema[0], norm), max(factor_norm_extrema[1], norm)]
        projected = tuple(math.trunc(f32(processed[i] * f)) for f in model[2:])
        if any(v < -32768 or v > 32767 for v in projected):
            raise ValueError('unsafe recorded projection')
        if projected != record[:3]:
            projection_mismatch.append(k)
        if m['calibrated']:
            # Record both ordinary float32 stages and possible compiler FMA.
            # A mismatch is evidence, not permission to alter the saved depths.
            a = f32(f32(raw[i] * model[0]) + model[1])
            b = f32(raw[i] * model[0] + model[1])
            if processed[i] not in (math.trunc(a), math.trunc(b)):
                processing_mismatch.append(k)
        elif processed[i] != raw[i]:
            processing_mismatch.append(k)
    return {'manifest': m, 'hashes': hashes, 'indices': indices, 'raw': raw,
            'processed': processed, 'records': records, 'models': models, 'flags': flags,
            'domain': list(domain), 'pixel_status': status, 'status_names': STATUS_NAMES,
            'report': {'filter_policy': m.get('filter', {}).get('policy', 'unfiltered_v1'),
                       'excluded_sample_count': candidates - n,
                       'status_counts': {name: status.count(s) for s, name in enumerate(STATUS_NAMES)},
                       'projection_mismatch_records': projection_mismatch,
                                             'processing_mismatch_records': processing_mismatch,
                                             'factor_norm_extrema': factor_norm_extrema if n else None,
                                             'zero_y_factor_records': zero_y_factors,
                                             'calibration_arithmetic': 'float32 staged / possible compiler FMA; not a sensor-unit equality claim'}}


def selected_csv(bundle, ids):
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(['record_id', 'source_index', 'x', 'y', 'sensor_depth', 'processed_depth',
                     'gain', 'offset', 'x_factor', 'y_factor', 'z_factor', 'X', 'Y', 'Z', 'D', 'cleanup_flag',
                     'projected_float_X', 'projected_float_Y', 'projected_float_Z', 'factor_norm',
                     'gain_offset_result_float32_staged', 'projection_matches_captured_model'])
    projection_mismatches = set(bundle['report']['projection_mismatch_records'])
    for k in ids:
        i = bundle['indices'][k]
        writer.writerow([k, i, i % 640, i // 640, bundle['raw'][i], bundle['processed'][i],
                         *bundle['models'][k], *bundle['records'][k], bundle['flags'][k],
                         *(f32(bundle['processed'][i] * f) for f in bundle['models'][k][2:]),
                         math.sqrt(sum(f*f for f in bundle['models'][k][2:])),
                         f32(f32(bundle['raw'][i] * bundle['models'][k][0]) + bundle['models'][k][1]) if bundle['manifest']['calibrated'] else '',
                         k not in projection_mismatches])
    return output.getvalue().encode()


def validate_png(data):
    if len(data) > 16 * 1024 * 1024 or data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('invalid/oversized PNG')
    position, compressed, dimensions = 8, bytearray(), None
    ended = False
    while position < len(data):
        if position + 12 > len(data):
            raise ValueError('truncated PNG chunk')
        size, = struct.unpack_from('>I', data, position)
        kind = data[position + 4:position + 8]
        end = position + 12 + size
        if end > len(data):
            raise ValueError('truncated PNG payload')
        payload = data[position + 8:end - 4]
        crc, = struct.unpack_from('>I', data, end - 4)
        if zlib.crc32(kind + payload) != crc:
            raise ValueError('PNG CRC mismatch')
        if dimensions is None:
            if kind != b'IHDR' or size != 13:
                raise ValueError('missing PNG header')
            w, h, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', payload)
            if not 1 <= w <= 8192 or not 1 <= h <= 8192 or w * h > 20_000_000 or depth != 8 or color not in (2, 6) or compression or filtering or interlace:
                raise ValueError('unsupported/oversized PNG dimensions/encoding')
            dimensions = (w, h)
            row = w * (3 if color == 2 else 4) + 1
        elif kind == b'IHDR':
            raise ValueError('duplicate PNG header')
        if kind == b'IDAT':
            compressed.extend(payload)
        if kind == b'IEND':
            if size or end != len(data):
                raise ValueError('invalid PNG terminator')
            ended = True
        position = end
    if not ended or not dimensions:
        raise ValueError('incomplete PNG')
    decoder = zlib.decompressobj()
    pixels = decoder.decompress(compressed, row * dimensions[1] + 1)
    if len(pixels) != row * dimensions[1] or not decoder.eof or decoder.unused_data or any(pixels[i] > 4 for i in range(0, len(pixels), row)):
        raise ValueError('invalid PNG pixel stream')
    return dimensions


def save_render(bundle, output, request):
    ids = request['selection']['record_ids']
    if (type(ids) is not list or len(ids) > LIMIT or
        any(type(k) is not int or not 0 <= k < len(bundle['records']) for k in ids) or len(set(ids)) != len(ids)):
        raise ValueError('invalid selected record IDs')
    images = request['images']
    if set(images) != IMAGES:
        raise ValueError('required paired images missing')
    decoded = {}
    for name, encoded in images.items():
        data = base64.b64decode(encoded, validate=True)
        w, h = validate_png(data)
        if name.startswith(('sensor-', 'processed-')) and (w, h) != (640, 560):
            raise ValueError('depth export must retain 640x480 pixels plus 80px footer')
        decoded[name] = data
    # Select a new directory; never let an HTTP payload supply a filesystem path.
    for number in range(1, 10001):
        directory = output / ('export_%04d' % number)
        try:
            directory.mkdir(mode=0o700)
            break
        except FileExistsError:
            continue
    else:
        raise ValueError('export count cap reached')
    (directory / 'INCOMPLETE').touch()
    for name, data in decoded.items():
        (directory / name).write_bytes(data)
    selection = {**request['selection'], 'capture_hashes': bundle['hashes'],
                 'frame_id': bundle['manifest']['frame_id'], 'record_id_base': 0}
    (directory / 'selection.json').write_text(json.dumps(selection, indent=2) + '\n')
    (directory / 'selected-pixels.csv').write_bytes(selected_csv(bundle, ids))
    with (directory / 'excluded-pixels.csv').open('w', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['source_index', 'x', 'y', 'sensor_depth', 'processed_depth', 'reason'])
        for i, status in enumerate(bundle['pixel_status']):
            if status > 1:
                writer.writerow([i, i%640, i//640, bundle['raw'][i], bundle['processed'][i], STATUS_NAMES[status]])
    (directory / 'numerical-report.json').write_text(json.dumps(bundle['report'], indent=2) + '\n')
    manifest = {'schema': 1, 'tool': 'tools/pointcloud_capture.py',
                'capture_manifest': bundle['manifest'], 'capture_hashes': bundle['hashes'],
                'sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in directory.iterdir() if p.name != 'INCOMPLETE'}}
    (directory / 'render-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (directory / 'INCOMPLETE').unlink()
    return directory


def serve(bundle, output, port=0):
    token = secrets.token_hex(24)
    # Nanosecond process identity is not representable as a JavaScript Number.
    viewer_bundle = {**bundle, 'manifest': {**bundle['manifest'], 'process_started_ns': str(bundle['manifest']['process_started_ns'])}}
    payload = json.dumps(viewer_bundle, separators=(',', ':')).encode()
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def valid_host(self):
            return self.headers.get('Host') == f'127.0.0.1:{self.server.server_port}'

        def reply(self, status, data, kind='application/json'):
            self.send_response(status)
            self.send_header('Content-Type', kind)
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if not self.valid_host():
                return self.reply(403, b'{}')
            if self.path == '/capture':
                return self.reply(200, payload)
            if self.path == '/':
                page = (ROOT / 'tools/pointcloud-capture.html').read_text().replace('__TOKEN__', token)
                return self.reply(200, page.encode(), 'text/html; charset=utf-8')
            if self.path == '/pointcloud-capture.js':
                return self.reply(200, (ROOT / 'tools/pointcloud-capture.js').read_bytes(), 'application/javascript')
            if self.path[1:] in ASSETS:
                return self.reply(200, (ROOT / 'release/files' / self.path[1:]).read_bytes(), 'application/javascript')
            self.reply(404, b'{}')

        def do_POST(self):
            origin = f'http://127.0.0.1:{self.server.server_port}'
            if (not self.valid_host() or self.path != '/export' or self.headers.get('Origin') != origin or
                self.headers.get('X-Capture-Token') != token or self.headers.get('Transfer-Encoding')):
                return self.reply(403, b'{}')
            try:
                size = int(self.headers.get('Content-Length', '-1'))
                if not 0 < size <= 96 * 1024 * 1024:
                    raise ValueError('request byte cap')
                self.connection.settimeout(30)
                body = self.rfile.read(size)
                if len(body) != size:
                    raise ValueError('incomplete request')
                result = save_render(bundle, output, json.loads(body))
                self.reply(200, json.dumps({'directory': str(result)}).encode())
            except (ValueError, KeyError, TypeError, OSError, struct.error) as error:
                self.reply(400, json.dumps({'error': str(error)}).encode())
    server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    server.daemon_threads = True
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--out', type=Path, help='New host output directory; required to serve viewer')
    parser.add_argument('--validate-only', action='store_true')
    args = parser.parse_args()
    bundle = load_bundle(args.capture)
    if args.validate_only:
        print(json.dumps({'records': len(bundle['records']), 'report': bundle['report']}, indent=2))
        return
    if args.out is None:
        parser.error('--out NEW_DIRECTORY is required for the viewer')
    args.out.mkdir(mode=0o700, parents=False, exist_ok=False)
    server = serve(bundle, args.out.resolve())
    print(f'Recorded capture viewer: http://127.0.0.1:{server.server_port}/', flush=True)
    print('Local files only; Ctrl-C stops the viewer. No camera connection.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
