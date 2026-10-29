#!/usr/bin/env python3
"""Host-only comparison from validated capture + replay masks. No dependencies.
Run pointcloud_capture.py --validate-only first. Output must be a new directory.
HTML preserves record IDs/source coordinates; click any view to inspect a record.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('capture', type=Path)
    p.add_argument('replay', type=Path)
    p.add_argument('output', type=Path)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    records = list(struct.iter_unpack('<hhhh', (a.capture / 'cloud.i16le').read_bytes()))
    indices = [v[0] for v in struct.iter_unpack('<I', (a.capture / 'source-index.u32le').read_bytes())]
    names = ['original', 'center50', 'r4-v1-island0', 'r6-v1-island0', 'r6-v1-island75', 'aggressive-default']
    flags = [[0] * len(indices)] + [list((a.replay / (name + '.u8')).read_bytes()) for name in names[1:]]
    assert len(records) == len(indices) and all(len(f) == len(indices) for f in flags)
    payload = dict(records=records, indices=indices, names=names, flags=flags)
    with (a.output / 'records.csv').open('w') as f:
        w = csv.writer(f)
        w.writerow(['record', 'source_x', 'source_y', 'X', 'Y', 'Z', 'D'] + names[1:])
        for k, (i, r) in enumerate(zip(indices, records)):
            w.writerow([k, i % 640, i // 640, *r] + [v[k] for v in flags[1:]])
    # Explicit analyst regions, not claims that every member is an artifact.
    # Stable controls are numerical 16x16 depth patches, NOT a labeled wall/body.
    boxes = {'finger_transition': (315, 350, 390, 425),
             'hand_context': (140, 365, 350, 440),
             'left_mid_scene': (60, 90, 180, 260),
             'right_mid_scene': (530, 570, 160, 240),
             'stable_left': (80, 95, 312, 327),
             'stable_right': (568, 583, 232, 247),
             'stable_foreground': (144, 159, 384, 399)}
    groups = {name: [k for k, i in enumerate(indices) if x0 <= i % 640 <= x1 and y0 <= i // 640 <= y1]
              for name, (x0, x1, y0, y1) in boxes.items()}
    groups['illustrative_edges'] = [k for k, expected in [(32928, 401 * 640 + 325), (33870, 412 * 640 + 339)]
                                    if k < len(indices) and indices[k] == expected]
    group_report = {}
    for name, ids in groups.items():
        group_report[name] = {'source_box_inclusive': boxes.get(name), 'records': len(ids),
                              'labels': 'analyst illustrative region, not bad-pixel ground truth',
                              'policies': {}}
        for policy, f in zip(names[1:], flags[1:]):
            rejected = [k for k in ids if f[k] == 8]
            retained = [k for k in ids if f[k] != 8]
            group_report[name]['policies'][policy] = {'suspects': len(rejected), 'visible': len(retained)}
            (a.output / (name + '-' + policy + '.json')).write_text(json.dumps({
                'rejected_record_ids': rejected, 'visible_record_ids': retained}))
    (a.output / 'groups.json').write_text(json.dumps(group_report, indent=2))
    html = '''<!doctype html><meta charset="utf-8"><title>Aggressive cleanup replay</title>
<style>body{background:#171b21;color:white;font:14px sans-serif}canvas{background:#000;border:1px solid #555}h2{margin:12px 0 4px}pre{white-space:pre-wrap}</style>
<h1>arm2 saved-frame comparison · original XYZD preserved</h1>
<p>Analyst projection, not registered to top2.png. Hide only flag 8. Color = processed depth, fixed 250..6000 mm. Source map uses exact emitted pixels. Click for record/source identity.</p><pre id="pick">No per-record ground truth inferred from screenshot colors.</pre><div id="views"></div>
<script>const data=PAYLOAD;
const color=d=>`hsl(${240-240*Math.max(0,Math.min(1,(d-250)/5750))},100%,55%)`;
window.outputs=[];
for(let n=0;n<data.names.length;n++){
 const h=document.createElement('h2');h.textContent=data.names[n]+': '+data.flags[n].filter(v=>v===8).length+' suspects / '+data.records.length;views.append(h);
 for(const type of ['top','front','source']){
  const wrap=document.createElement('span'),c=document.createElement('canvas');c.width=640;c.height=480;wrap.append(c);views.append(wrap);const g=c.getContext('2d');g.fillStyle='black';g.fillRect(0,0,640,480);
  const coords=data.records.map((r,k)=>type==='source'?[data.indices[k]%640,Math.floor(data.indices[k]/640)]:[320+r[0]*.065, type==='top'?240+r[2]*.065:300-r[1]*.12]);
  data.records.forEach((r,k)=>{if(data.flags[n][k]===8)return;g.fillStyle=color(r[3]);g.fillRect(coords[k][0],coords[k][1],1.5,1.5)});
  g.fillStyle='white';g.font='14px sans-serif';g.fillText(type+' · '+data.names[n],10,20);
  c.onclick=e=>{const b=c.getBoundingClientRect(),x=(e.clientX-b.x)*640/b.width,y=(e.clientY-b.y)*480/b.height;let best=-1,dist=25;coords.forEach((p,k)=>{if(data.flags[n][k]===8)return;const d=(p[0]-x)**2+(p[1]-y)**2;if(d<dist){dist=d;best=k}});if(best>=0)document.getElementById('pick').textContent=JSON.stringify({record:best,source:[data.indices[best]%640,Math.floor(data.indices[best]/640)],XYZD:data.records[best],flags:data.names.map((v,k)=>[v,data.flags[k][best]])});};
  outputs.push({name:data.names[n]+'-'+type+'.png',canvas:c});
 }
}
window.CONSOLE_READY=true;</script>'''.replace('PAYLOAD', json.dumps(payload, separators=(',', ':')))
    (a.output / 'comparison.html').write_text(html)
    (a.output / 'provenance.json').write_text(json.dumps({
        'capture_manifest_sha256': hashlib.sha256((a.capture / 'capture.json').read_bytes()).hexdigest(),
        'masks': {name: hashlib.sha256((a.replay / (name + '.u8')).read_bytes()).hexdigest() for name in names[1:]},
        'labels': 'Screenshot-guided analyst comparison; not GPU-picked per-record ground truth',
        'depth_color_mm': [250, 6000], 'original_geometry_unchanged': True,
    }, indent=2))


if __name__ == '__main__':
    main()
