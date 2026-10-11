"""F09 global modes, observational reads, bounded freshness and SDK aliases."""
import json
from pathlib import Path
import time


def exercise(request, directory):
    def data(path, status=200):
        return json.loads(request(path, status))

    pause = Path(directory) / 'capture.pause'
    fault = Path(directory) / 'lifecycle.fail'
    try:
        for mode in range(7):
            setting = data(f'/colormode/setting/{mode}')
            # Require a frame published after the setting request. A latest-frame
            # read can otherwise observe an older frame that already happened to
            # have the requested mode from a prior transition.
            frame = data('/frame/camera?after=' + str(setting['frame_id']))
            deadline = time.monotonic() + 3
            while frame['color_mode'] != mode:
                frame = data('/frame/camera?after=' + str(frame['frame_id']))
                assert time.monotonic() < deadline
            revision = frame['configuration_revision']
            # Separate connections stand in for a second browser/tab. Every
            # shipped homepage alias and HEAD remain purely observational.
            for path in ('/', '/app.html', '/app.html?cache=other-viewer'):
                request(path)
                request(path, method='HEAD')
            assert data('/colormode/setting')['value'] == mode
            assert data('/frame/camera')['configuration_revision'] == revision
            request('/raw/camera')
            if mode != 4:
                for action in ('pointcloud', 'pointcloud_obj'):
                    error = data('/' + action + '/camera', 409)
                    assert error['current_mode'] == mode
            else:
                request('/pointcloud/camera?after=0')
                request('/pointcloud_obj/camera?after=0')
                assert data('/image/camera', 409)['current_mode'] == 4
            assert data('/colormode/setting')['value'] == mode

        data('/colormode/setting/1')
        old = data('/frame/camera')['frame_id']
        for action in ('frame', 'raw', 'image'):
            _, headers = request(f'/{action}/camera?after={old}&timeout_ms=1000', with_headers=True)
            if action != 'frame':
                assert int(headers['x-chiton-frame-id']) > old
                assert int(headers['x-chiton-frame-age-ms']) >= 0
                assert headers['x-chiton-color-mode'] == '1'
            old = data('/frame/camera')['frame_id']
        # Real stalled publication, not a disabled camera or repeated constant pixels.
        pause.touch(); time.sleep(.06)
        frozen = data('/frame/camera')
        request('/image/camera')  # Ordinary latest-frame reads can be stale.
        for action in ('frame', 'raw', 'image'):
            start = time.monotonic()
            data(f'/{action}/camera?after={frozen["frame_id"]}&timeout_ms=100', 504)
            assert .07 <= time.monotonic() - start < .7
        start = time.monotonic()
        data('/frame/camera?after=18446744073709551615', 504)
        assert .9 <= time.monotonic() - start < 1.7
        data(f'/frame/camera?after={frozen["frame_id"]}&timeout_ms=0', 504)
        latest = data('/frame/camera')
        assert latest['frame_id'] == frozen['frame_id']
        assert latest['frame_age_ms'] >= frozen['frame_age_ms'] + 1000
        for query in ('after=-1', 'after=1x', 'after=', 'after=18446744073709551616',
                      'after=1&after=2', 'after=1&timeout_ms=1001', 'timeout_ms=1',
                      'after=1&timeout_ms=-1', 'after=1&timeout_ms=1&timeout_ms=2', 'after=1&'):
            data('/frame/camera?' + query, 400)
        pause.unlink()
        assert data('/frame/camera?after=' + str(frozen['frame_id']))['frame_id'] > frozen['frame_id']

        for disable, enable in (('/disable/camera', '/camera/setting/enable'),
                                ('/camera/setting/disable', '/enable/camera')):
            assert data(disable)['value'] is False
            assert data('/camera/setting')['value'] is False
            for action in ('frame', 'image', 'raw', 'pointcloud', 'pointcloud_obj'):
                start = time.monotonic()
                data('/' + action + '/camera?after=0', 503)
                assert time.monotonic() - start < .5
            fault.touch()
            data(enable, 500)
            assert data('/camera/setting')['value'] is False
            fault.unlink()
            assert data(enable)['value'] is True
            fault.touch()
            data(disable, 500)
            assert data('/camera/setting')['value'] is True
            fault.unlink()
            request('/frame/camera?after=0')
    finally:
        pause.unlink(missing_ok=True)
        fault.unlink(missing_ok=True)
    print('Observational reads, global modes, freshness and lifecycle aliases passed')
