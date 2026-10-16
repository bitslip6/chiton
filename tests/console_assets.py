#!/usr/bin/env python3
"""Flat console runtime references, package inclusion and untouched legacy parity."""
import importlib.util
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'release/files'
spec = importlib.util.spec_from_file_location('package_aarch64', ROOT / 'tools/package_aarch64.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)
html = (ASSETS / 'console.html').read_text()
references = re.findall(r'(?:src|href)="/([^"?#]+)"', html)
runtime = [p for p in references if p != 'app.html']
assert len(re.findall(r'\bid="([^"]+)"', html)) == len(set(re.findall(r'\bid="([^"]+)"', html))), 'Duplicate IDs'
assert (ASSETS / 'app.html').read_bytes() == (ROOT / 'release/app.html').read_bytes()
for name in runtime:
    assert '/' not in name, f'Non-flat asset: {name}'
    assert (ASSETS / name).is_file(), name
assert 'console-app.js' not in (ASSETS / 'app.html').read_text()
with tempfile.TemporaryDirectory(prefix='chiton-console-package-') as directory:
    target = Path(directory) / 'files'
    packager.copy_assets(ASSETS, target)
    for name in runtime + ['console.html']:
        assert (target / name).read_bytes() == (ASSETS / name).read_bytes(), name
print(f'console_assets: {len(runtime)} flat references, bundle inclusion, IDs and legacy parity passed')
