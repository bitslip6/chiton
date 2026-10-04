"""Selected shipped-UI response schemas, after simulator identification only."""
import json
import math


def exercise(request):
    def data(path, status=200):
        return json.loads(request(path, status))

    dimensions = data('/dimension/camera')
    assert set(dimensions) == {'inner_dist', 'outer_dist', 'center_x', 'center_y'}
    assert all(type(value) is int for value in dimensions.values())
    assert 0 <= dimensions['center_x'] < 640 and 0 <= dimensions['center_y'] < 480
    radians = data('/radians/setting')
    assert set(radians) == {'radian' + str(i) for i in range(1, 9)}
    assert all(isinstance(value, (int, float)) and math.isfinite(value) for value in radians.values())
    for action in ('dynamic', 'chitoncal', 'smoothing'):
        accepted = data('/' + action + '/setting/off')
        assert accepted['value'] is False
        if action == 'smoothing':
            assert accepted['setting'] == 'smoothing'
    assert data('/quality/setting/75') == {'value': 75}
    assert data('/colormode/setting/1')['value'] == 1
    frame_type = data('/depth/setting')
    assert type(frame_type['value']) is bool and frame_type['frame_type'] in ('depth', 'ir')
    assert frame_type['value'] == (frame_type['frame_type'] == 'depth')
    color_range = data('/range/setting')
    assert type(color_range['min']) is int and type(color_range['max']) is int
    assert 0 <= color_range['min'] < color_range['max'] <= 9000
    temperature = data('/temp/setting')
    assert all(isinstance(temperature[field], (int, float)) and math.isfinite(temperature[field])
               for field in ('cputemp', 'vixeltemp'))

    # These are simulator rejections, not real provisioning/upgrade/diagnostic
    # operations. The parent suite refuses a hardware build before this call.
    for route in ('/gateway/setting/eth0', '/wlan0/setting/mock-network/mock-password',
                  '/upgrade/software/32', '/debug/software'):
        rejected = data(route, 501)
        assert rejected['status'] == 501 and isinstance(rejected['error'], str)
    print('HTTP shipped UI schemas passed')
