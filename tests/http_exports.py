"""F13 export checks; called only after HTTP suite verifies a simulator build.

Constant fixtures and disabled calibration/smoothing allow cloud and OBJ requests
on distinct frames to be compared without assuming equal frame IDs.
"""
from array import array
import math
import sys
import time


def exercise(request):
    contracts = {
        'raw': ('chiton-raw.bin', 'application/octet-stream', 'processed_uint16_row_major'),
        'pointcloud': ('chiton-pointcloud.bin', 'application/octet-stream', 'int16_xyzd'),
        'pointcloud_obj': ('chiton-pointcloud.obj', 'text/plain; charset=utf-8', 'obj_xyz_divided_by_6000'),
    }
    exports = {}
    for action, (filename, mime, layout) in contracts.items():
        if action == 'pointcloud': request('/colormode/setting/4')
        body, headers = request('/' + action + '/camera?after=0', with_headers=True)
        assert headers['content-disposition'] == f'attachment; filename="{filename}"'
        assert headers['content-type'] == mime
        assert headers['connection'] == 'close'
        assert headers['cache-control'] == 'no-store'
        assert headers['x-chiton-layout'] == layout
        assert int(headers['x-chiton-frame-id']) > 0
        assert int(headers['x-chiton-captured-at-ms']) > 0
        assert int(headers['content-length']) == len(body)
        assert headers['x-chiton-invalid-sample-policy'] == 'all_uint16_included'
        exports[action] = body, headers

    raw, headers = exports['raw']
    assert len(raw) == 640 * 480 * 2
    assert headers['x-chiton-width'] == '640' and headers['x-chiton-height'] == '480'
    words = array('H'); words.frombytes(raw)
    if headers['x-chiton-byte-order'] != sys.byteorder:
        words.byteswap()
    # The transition to DEPTH may not yet have published when raw is read; either
    # complete constant fixture is valid, never a truncated/mixed/sentinel frame.
    assert set(words) in ({800}, {1800})

    cloud, cloud_headers = exports['pointcloud']
    assert len(cloud) > 0 and len(cloud) % 8 == 0
    points = array('h'); points.frombytes(cloud)
    if cloud_headers['x-chiton-byte-order'] != sys.byteorder:
        points.byteswap()
    assert int(cloud_headers['x-chiton-width']) == len(points) // 4
    assert cloud_headers['x-chiton-height'] == '1'

    obj, obj_headers = exports['pointcloud_obj']
    assert obj_headers['x-chiton-byte-order'] == 'not_applicable'
    assert obj_headers['x-chiton-units'] == 'normalized_6000_' + cloud_headers['x-chiton-units']
    lines = obj.decode('ascii').splitlines()
    assert len(lines) == len(points) // 4 == int(obj_headers['x-chiton-width'])
    assert obj_headers['x-chiton-height'] == '1'
    for index, line in enumerate(lines):
        fields = line.split()
        assert len(fields) == 4 and fields[0] == 'v'
        for component, value in enumerate(fields[1:]):
            value = float(value)
            assert math.isfinite(value)
            assert abs(value - points[index * 4 + component] / 6000) <= .000051

    # Live JPEG polling is not an attachment and remains a real JPEG, not PNG.
    request('/colormode/setting/1')
    # Switching out of cloud mode can briefly leave no JPEG.
    # Accept the documented 503 until a newly encoded frame publishes.
    deadline = time.monotonic() + 3
    while True:
        jpeg, headers = request('/image/camera', (200, 503), with_headers=True)
        if headers['content-type'] == 'image/jpeg':
            break
        assert 'content-disposition' not in headers
        assert time.monotonic() < deadline, 'JPEG did not recover after mode switch'
        time.sleep(.02)
    assert headers['content-type'] == 'image/jpeg'
    assert jpeg.startswith(b'\xff\xd8') and jpeg.endswith(b'\xff\xd9')
    assert 'content-disposition' not in headers

    # Export routes retain the existing GET-only method policy. A HEAD must not
    # acquire a cloud frame or return an attachment, even while capture is live.
    for action in contracts:
        body, headers = request('/' + action + '/camera', 405, method='HEAD', with_headers=True)
        assert body == b'' and 'content-disposition' not in headers

    # Errors must remain ordinary JSON errors, not downloadable binary files.
    request('/disable/camera')
    for action in (*contracts, 'image'):
        _, headers = request('/' + action + '/camera', 503, with_headers=True)
        assert 'application/json' in headers['content-type']
        assert 'content-disposition' not in headers
    request('/enable/camera')
    print('HTTP export contracts passed')
