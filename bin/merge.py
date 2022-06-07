#!/bin/python

import json
import copy
import sys


#with open('/usr/local/chiton/current/config.bak.json') as f:
with open(sys.argv[1]) as f:
    v1 = json.load(f)
#with open('/usr/local/chiton/current/config.json') as f:
with open(sys.argv[2]) as f:
    v2 = json.load(f)


m2 = copy.deepcopy(v2)

for i in range(0, 8):

    # because Juan decided integers were better represnted as strings, we have to handle both
    m2['measurements'][i]['pre_gain'] = v1['measurements'][i]['pre_gain']
    m2['measurements'][i]['point1']   = v1['measurements'][i]['point1']
    m2['measurements'][i]['point2']   = v1['measurements'][i]['point2']
    m2['measurements'][i]['target1']  = v1['measurements'][i]['target1']
    m2['measurements'][i]['target2']  = v1['measurements'][i]['target2']

m2['center_x'] = v1['center_x']
m2['center_y'] = v1['center_y']
m2['inner_dist'] = v1['inner_dist']
m2['outer_dist'] = v1['outer_dist']


#with open('/usr/local/chiton/current/config.new.json', 'w') as f:
with open(sys.argv[3], 'w') as f:
    json.dump(m2, f, indent=4)
