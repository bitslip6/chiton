#!/usr/bin/env python3
"""Bounded, local-only Chiton OBJ/XYZD forensic analysis. Python standard library only."""
import argparse
import hashlib
import html
import json
import math
from pathlib import Path
import struct

MAX_RECORDS = 640 * 480
MAX_BINARY = MAX_RECORDS * 8
MAX_OBJ = 24 * 1024 * 1024


def local_bytes(path, cap):
    path = Path(path)
    if not path.is_file():
        raise ValueError(f"not a local regular file: {path}")
    with path.open('rb') as stream:
        body = stream.read(cap + 1)
    if len(body) > cap:
        raise ValueError(f"file exceeds {cap} byte cap")
    return body


def parse_obj(body, recover=False):
    """Chiton export scale, not meters. Recovery is opt-in and lattice-checked."""
    if len(body) > MAX_OBJ:
        raise ValueError('OBJ byte cap exceeded')
    records = []
    for number, line in enumerate(body.decode('ascii').splitlines(), 1):
        if len(line) > 1024:
            raise ValueError(f'OBJ line {number} too long')
        fields = line.split('#', 1)[0].split()
        if not fields:
            continue
        if len(fields) != 4 or fields[0] != 'v':
            raise ValueError(f'expected vertex-only OBJ at line {number}')
        xyz = [float(v) * 6000 for v in fields[1:]]
        # Avoid overflow downstream; this tool accepts only Chiton's signed export domain.
        if any(not math.isfinite(v) or abs(v) > 32768.4 for v in xyz):
            raise ValueError(f'nonfinite/out-of-contract coordinate at line {number}')
        if recover:
            if any(abs(v - round(v)) > .300001 or not -32768 <= round(v) <= 32767 for v in xyz):
                raise ValueError('OBJ not compatible with four-decimal signed-int16 export lattice')
            xyz = [round(v) for v in xyz]
        if len(records) >= MAX_RECORDS:
            raise ValueError('OBJ record cap exceeded')
        records.append(tuple(xyz))
    return records


def parse_binary(body, headers):
    if len(body) > MAX_BINARY or len(body) % 8:
        raise ValueError('binary length/alignment/cap failure')
    h = {k.lower(): str(v) for k, v in headers.items()}
    def get(name):
        return h.get('x-chiton-' + name.lower())
    def integer(name, minimum=0):
        value = get(name)
        if value is None or not value.isascii() or not value.isdecimal():
            raise ValueError('missing/invalid ' + name)
        value = int(value)
        if not minimum <= value <= 2**64 - 1:
            raise ValueError('out-of-range ' + name)
        return value
    if h.get('content-type', '').split(';')[0].strip().lower() != 'application/octet-stream':
        raise ValueError('expected application/octet-stream')
    if get('layout') != 'int16_xyzd' or get('color-mode') != '4':
        raise ValueError('incompatible layout/mode')
    if get('frame-type') != 'depth':
        raise ValueError('only depth geometry is supported')
    units, scope = get('units'), get('units-scope')
    if units not in ('mm', 'depth_sensor_units') or get('source-units') != units:
        raise ValueError('inconsistent units')
    if scope != ('calibration_aoi' if units == 'mm' else 'full_sensor'):
        raise ValueError('inconsistent scope')
    if get('invalid-sample-policy') != 'all_uint16_included':
        raise ValueError('unknown sample policy')
    order = get('byte-order')
    if order not in ('little', 'big'):
        raise ValueError('missing/invalid byte order')
    count = integer('width')
    if count > MAX_RECORDS or integer('height') != 1 or count * 8 != len(body):
        raise ValueError('record count/body mismatch')
    if 'content-length' in h and h['content-length'] != str(len(body)):
        raise ValueError('content-length mismatch')
    integer('frame-id', 1)
    integer('configuration-revision')
    for name in ('captured-at-ms', 'frame-age-ms'):
        if get(name) is not None:
            integer(name)
    return list(struct.iter_unpack(('<' if order == 'little' else '>') + 'hhhh', body))


def distribution(values):
    if not values:
        return {'count': 0, 'min': None, 'max': None, 'histogram': []}
    lo, hi = min(values), max(values)
    bins = [0] * 32
    for v in values:
        bins[min(31, int((v - lo) / (hi - lo) * 32)) if hi != lo else 0] += 1
    ordered = sorted(values)
    return {'count': len(values), 'min': lo, 'max': hi,
            'p05': ordered[int((len(values)-1)*.05)], 'median': ordered[(len(values)-1)//2],
            'p95': ordered[int((len(values)-1)*.95)],
            'histogram': bins, 'histogram_equal_width_bins': 32}


def analyze(records, near_y=1.0):
    radii = [math.hypot(*p[:3]) for p in records]
    nonzero = [(p, r) for p, r in zip(records, radii) if r > 0]
    result = {
        'record_count': len(records), 'first_record': records[0] if records else None,
        'xyz': {axis: distribution([p[k] for p in records]) for k, axis in enumerate('XYZ')},
        'radius': distribution(radii), 'origin_count': radii.count(0),
        'near_origin_le_1': sum(r <= 1 for r in radii),
        'zero_y_count': sum(p[1] == 0 for p in records),
        'zero_y_nonorigin_count': sum(p[1] == 0 and r > 0 for p, r in zip(records, radii)),
        'zero_y_nonorigin_radius': distribution([r for p, r in zip(records, radii) if p[1] == 0 and r > 0]),
        'near_y_tolerance': near_y, 'near_y_count': sum(abs(p[1]) <= near_y for p in records),
        'elevation_degrees': distribution([math.degrees(math.atan2(p[1], math.hypot(p[0], p[2]))) for p, _ in nonzero]),
        'azimuth_degrees': distribution([math.degrees(math.atan2(p[0], p[2])) for p, _ in nonzero]),
        'angular_excluded_origin_count': len(records) - len(nonzero),
    }
    result['direction_bins'] = []
    for width in (.5, 1.0, 2.0):
        bins = {}
        for p, r in nonzero:
            az = math.degrees(math.atan2(p[0], p[2]))
            el = math.degrees(math.atan2(p[1], math.hypot(p[0], p[2])))
            key = (math.floor(az / width), math.floor(el / width))
            b = bins.setdefault(key, [0, r, r])
            b[0] += 1
            b[1], b[2] = min(b[1], r), max(b[2], r)
        broad = sorted(bins.items(), key=lambda item: item[1][2]-item[1][1], reverse=True)[:20]
        result['direction_bins'].append({'width_degrees': width, 'bin_origin_degrees': 0,
            'occupied': len(bins), 'largest_radial_spans': [
                {'azimuth_bin': k[0], 'elevation_bin': k[1], 'count': b[0],
                 'radius_min': b[1], 'radius_max': b[2], 'span': b[2]-b[1]} for k, b in broad]})
    if records and len(records[0]) == 4:
        depth = [p[3] for p in records]
        result['depth'] = distribution(depth)
        result['negative_depth_count'] = sum(d < 0 for d in depth)
        result['zero_depth_count'] = depth.count(0)
        result['norm_minus_positive_signed_depth'] = distribution([r-p[3] for p, r in zip(records, radii) if p[3] > 0])
    else:
        result['depth'] = None
    return result


def scatter_svg(records, color='uniform', limit=12000):
    """Equal-axis orthographic point plots. Downsample plots only, never statistics."""
    stride = max(1, math.ceil(len(records) / limit))
    selected = list(range(0, len(records), stride))
    values = [math.hypot(*p[:3]) for p in records]
    if color == 'depth':
        values = [p[3] for p in records]
    elif color == 'order':
        values = list(range(len(records)))
    lo, hi = (min(values), max(values)) if values else (0, 1)
    legend = ('red: exact Y=0; blue: all other records' if color == 'zero-y' else
              'uniform point color' if color == 'uniform' else f'blue (min {lo:.2f}) to red (max {hi:.2f})')
    chunks = ['<svg xmlns="http://www.w3.org/2000/svg" width="1260" height="480" viewBox="0 0 1260 480">',
              '<rect width="1260" height="480" fill="#101820"/>',
              '<g fill="#e8eef2" font-family="sans-serif" font-size="13">',
              f'<text x="15" y="22">API coordinates; XYZ in source-unit scale, not gravity/world axes. Color: {html.escape(color)}</text>',
              f'<text x="15" y="44">Unfiltered dataset {len(records)} records; plot stride {stride}, {len(selected)} shown. No connections/mesh. {legend}</text>',
              '<text x="15" y="465">Highlight is diagnostic only; unchanged uniform baseline is in uniform.svg. Origin is API (0,0,0), not established sensor pose.</text></g>']
    for panel, (a, b) in enumerate(((0, 2), (0, 1), (1, 2))):
        x0 = panel * 420
        xs, ys = [p[a] for p in records], [p[b] for p in records]
        xmin, xmax = (min(xs), max(xs)) if xs else (-1, 1)
        ymin, ymax = (min(ys), max(ys)) if ys else (-1, 1)
        cx, cy = (xmin+xmax)/2, (ymin+ymax)/2
        scale = 335 / max(xmax-xmin, ymax-ymin, 1)
        chunks.append(f'<text x="{x0+20}" y="75" fill="white">{"XYZ"[a]} / {"XYZ"[b]} · horizontal / vertical</text>')
        for i in selected:
            p = records[i]
            fill = '#c7d9e8'
            if color == 'zero-y':
                fill = '#ff654f' if p[1] == 0 else '#5b899e'
            elif color != 'uniform':
                t = (values[i]-lo)/(hi-lo) if hi != lo else .5
                fill = f'hsl({240*(1-t):.1f},85%,65%)'
            x, y = x0+210+(p[a]-cx)*scale, 260-(p[b]-cy)*scale
            chunks.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r=".85" fill="{fill}"/>')
        chunks.append(f'<text x="{x0+15}" y="445" fill="white" font-size="11">ranges {xmin:.1f}..{xmax:.1f} / {ymin:.1f}..{ymax:.1f}</text>')
    chunks.append('</svg>')
    return '\n'.join(chunks)


def histogram_svg(report):
    names = ['radius', 'elevation_degrees', 'azimuth_degrees'] + (['depth'] if report['depth'] else [])
    chunks = ['<svg xmlns="http://www.w3.org/2000/svg" width="900" height="%d">' % (len(names)*210+50),
              '<rect width="100%" height="100%" fill="#101820"/>',
              '<text x="15" y="25" fill="white">All records: 32 equal-width bins; angles exclude origin only. Source-unit scale, API axes.</text>']
    for row, name in enumerate(names):
        dist = report[name]; y = row*210+55
        chunks.append(f'<text x="15" y="{y}" fill="white">{name}: count {dist["count"]}; min {dist["min"]}; max {dist["max"]}</text>')
        maximum = max(dist['histogram'], default=1) or 1
        for i, count in enumerate(dist['histogram']):
            height = count/maximum*150
            chunks.append(f'<rect x="{20+i*26}" y="{y+165-height:.2f}" width="24" height="{height:.2f}" fill="#79c6db"><title>bin {i}: {count}</title></rect>')
    return '\n'.join(chunks+['</svg>'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--out', type=Path, required=True, help='new evidence directory (never overwritten)')
    parser.add_argument('--headers', type=Path, help='JSON object containing this binary body\'s own response headers')
    parser.add_argument('--recover-export-integers', action='store_true', help='OBJ: XYZ * 6000 then nearest-integer rounding, lattice checked')
    parser.add_argument('--near-y', type=float, default=1.0)
    parser.add_argument('--note', default='', help='operator provenance/known gaps; never inferred')
    args = parser.parse_args()
    try:
        if not math.isfinite(args.near_y) or args.near_y < 0:
            raise ValueError('near-Y tolerance must be finite and nonnegative')
        binary = args.input.suffix.lower() == '.bin'
        if args.input.suffix.lower() not in ('.obj', '.bin'):
            raise ValueError('expected local .obj or .bin file')
        body = local_bytes(args.input, MAX_BINARY if binary else MAX_OBJ)
        header_bytes = None
        if binary:
            if not args.headers or args.recover_export_integers:
                raise ValueError('binary requires its own --headers and no OBJ recovery flag')
            header_bytes = local_bytes(args.headers, 65536)
            headers = json.loads(header_bytes)
            if not isinstance(headers, dict):
                raise ValueError('headers must be a JSON object')
            records = parse_binary(body, headers)
        else:
            if args.headers:
                raise ValueError('--headers is only for binary captures')
            headers = None
            records = parse_obj(body, args.recover_export_integers)
        report = analyze(records, args.near_y)
        manifest = {'input_path': str(args.input.resolve()), 'sha256': hashlib.sha256(body).hexdigest(),
            'bytes': len(body), 'format': 'signed_int16_xyzd' if binary else 'vertex_only_obj',
            'coordinate_conversion': 'none' if binary else ('XYZ * 6000; nearest integer (verified export lattice)' if args.recover_export_integers else 'XYZ * 6000; no integer recovery'),
            'obj_rounding_uncertainty_source_units': None if binary else .3,
            'headers': headers, 'headers_sha256': hashlib.sha256(header_bytes).hexdigest() if header_bytes is not None else None,
            'operator_note': args.note,
            'known_limits': ['No physical-camera access. No geometry/filter/model changes.',
                'API axes are not established gravity/world axes; negative XYZ is not invalid.',
                'Directional bin spans are descriptions, not proof of optical defects.',
                'OBJ has no depth/frame/pixel/calibration metadata; none inferred.' if not binary else 'Signed negative depth is preserved; no unsigned reinterpretation.',
                'Source pixel correspondence requires exact producer/configuration, not record order alone.']}
        args.out.mkdir(parents=True, exist_ok=False)
        (args.out / ('original.bin' if binary else 'original.obj')).write_bytes(body)
        if header_bytes is not None:
            (args.out / 'original-headers.json').write_bytes(header_bytes)
        (args.out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        (args.out / 'summary.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
        (args.out / 'distributions.svg').write_text(histogram_svg(report))
        for color in ('uniform', 'zero-y', 'radius', 'order') + (('depth',) if binary else ()):
            (args.out / (color + '.svg')).write_text(scatter_svg(records, color))
        print(json.dumps({'out': str(args.out), 'records': len(records), 'zero_y': report['zero_y_count'], 'sha256': manifest['sha256']}))
    except (ValueError, OSError, UnicodeError, TypeError) as error:
        parser.exit(2, f'analysis failed: {error}\n')


if __name__ == '__main__':
    main()
