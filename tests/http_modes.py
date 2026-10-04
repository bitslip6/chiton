"""All supported presentation modes, depth/IR transitions and capture recovery."""
import json
import time


def jpeg_dimensions(data):
    assert data[:2] == b'\xff\xd8' and data[-2:] == b'\xff\xd9'
    offset = 2
    while offset < len(data):
        assert data[offset] == 255
        while data[offset] == 255:
            offset += 1
        marker = data[offset]
        offset += 1
        length = int.from_bytes(data[offset:offset + 2], 'big')
        assert length >= 2 and offset + length <= len(data)
        if marker in (0xc0, 0xc1, 0xc2):
            return (int.from_bytes(data[offset + 5:offset + 7], 'big'),
                    int.from_bytes(data[offset + 3:offset + 5], 'big'))
        offset += length
    raise AssertionError('JPEG has no frame dimensions')


def exercise(request):
    for frame_type in ('ir', 'depth'):
        request('/depth/setting/' + frame_type)
        deadline = time.monotonic() + 8
        while json.loads(request('/frame/camera'))['frame_type'] != frame_type:
            assert time.monotonic() < deadline
            time.sleep(.02)
        # Visit every mode and both transition directions, including returning
        # from demand-driven cloud to continuous JPEG and planar back to polar.
        for mode in (0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0):
            previous = json.loads(request('/frame/camera'))['configuration_revision']
            assert json.loads(request(f'/colormode/setting/{mode}'))['value'] == mode
            if mode == 4:
                cloud = request('/pointcloud/camera')
                assert len(cloud) > 0 and len(cloud) % 8 == 0
            deadline = time.monotonic() + 8
            while True:
                metadata = json.loads(request('/frame/camera'))
                if metadata['configuration_revision'] > previous and metadata['frame_type'] == frame_type:
                    break
                assert time.monotonic() < deadline, metadata
                time.sleep(.02)
            dimensions = (1500, 165) if mode > 4 else (640, 480)
            width = json.loads(request('/width/setting'))
            assert (width['width'], width['height']) == dimensions
            raw, headers = request('/raw/camera', with_headers=True)
            assert len(raw) == 640 * 480 * 2
            assert headers['x-chiton-frame-type'] == frame_type
            if mode == 4:
                request('/image/camera', 503)
            else:
                image, headers = request('/image/camera', with_headers=True)
                assert jpeg_dimensions(image) == dimensions
                assert (int(headers['x-chiton-width']), int(headers['x-chiton-height'])) == dimensions
            # Invalid settings never switch the mode; freeze metadata in cloud
            # mode, rather than assuming requests observe a particular frame.
            for invalid in ('-1', '7', 'nan'):
                request('/colormode/setting/' + invalid, 400)
            assert json.loads(request('/frame/camera'))['configuration_revision'] == metadata['configuration_revision']
            request('/disable/camera')
            for path in ('/image/camera', '/raw/camera', '/pointcloud/camera',
                         '/pointcloud_obj/camera', '/frame/camera', '/wall_radian/camera', '/get/value/10/10'):
                request(path, 503)
            request('/dimension/camera')  # Configuration remains available.
            request('/enable/camera')
            if mode == 4:
                request('/pointcloud/camera')
            else:
                assert jpeg_dimensions(request('/image/camera')) == dimensions
    request('/colormode/setting/1')
    print('Depth/IR full-mode transitions and capture failure/recovery passed')
