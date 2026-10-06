#!/usr/bin/env python3
"""Inspect a generated application tarball without executing ARM code."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tarfile
import tempfile

archive = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='chiton-package-test-') as folder:
    with tarfile.open(archive) as tar:
        members = tar.getmembers()
        tops = {PurePosixPath(member.name).parts[0] for member in members}
        assert len(tops) == 1
        for member in members:
            assert not member.name.startswith('/') and '..' not in PurePosixPath(member.name).parts
            assert member.isfile() or member.isdir() or member.issym()
            if member.issym():
                assert (PurePosixPath(member.name).name, member.linkname) in {
                    ('app.html', 'files/app.html'), ('chiton', 'run.sh'), ('chiton.sh', 'run.sh')}
        tar.extractall(folder, filter='data')
    root = Path(folder)/tops.pop()
    manifest = json.loads((root/'manifest.json').read_text())
    assert manifest['architecture'] == 'aarch64' and manifest['camera'] == 'hardware'
    assert not (root/'chiton.json').exists() and not (root/'calibration.bin').exists()
    assert (root/'defaults/chiton.example.json').is_file()
    if 'image_transport' in manifest:
        assert (root/'files/image-stream.js').is_file()
    for required in ['files/app.html', 'files/debug.html', 'files/large_font.png', 'files/small_font.png', 'jabil', 'lib/libaditof.so.1.0']:
        assert (root/required).exists(), required
    for forbidden in ['setup.sh', 'upgrade.sh', 'dhcpd', 'chiton-debug', 'api.so', 'lib/libc.so.6', 'lib/libstdc++.so.6', 'files/app2.html', 'files/app3.html']:
        assert not (root/forbidden).exists(), forbidden
    for line in (root/'SHA256SUMS').read_text().splitlines():
        expected, name = line.split('  ', 1)
        assert hashlib.sha256((root/name).read_bytes()).hexdigest() == expected, name
    for name in manifest['bundled_libraries']:
        assert (root/'lib'/name).read_bytes()[:4] == b'\x7fELF'
    assert (root/'chiton.bin').read_bytes()[:4] == b'\x7fELF'
    for name, evidence in manifest.get('benchmarks', {}).items():
        assert name in {'image_pipeline_benchmark', 'calibration_pipeline_benchmark'}
        assert evidence['path'] == 'benchmarks/' + name
        program = root/evidence['path']
        assert program.read_bytes()[:4] == b'\x7fELF'
        assert os.access(program, os.X_OK)
        assert hashlib.sha256(program.read_bytes()).hexdigest() == evidence['sha256']
        assert evidence['camera_sdk'] is False
    if manifest.get('benchmarks'):
        assert len(manifest['benchmarks']) == 2
        for script in ('run.sh', 'run-all.sh'):
            path = root/'benchmarks'/script
            assert os.access(path, os.X_OK)
            subprocess.run(['sh', '-n', str(path)], check=True)
        assert (root/'benchmarks/README.md').is_file()
        assert (root/'benchmarks/pipeline-performance-metrics.md').is_file()
    assert (root/'chiton').resolve() == root/'run.sh'
    assert (root/'chiton.sh').resolve() == root/'run.sh'
    subprocess.run(['sh', '-n', str(root/'run.sh')], check=True)
    # A missing per-device config must fail before the ARM executable is invoked.
    env = dict(os.environ, CHITON_STATE_DIR=str(root/'absent-state'))
    result = subprocess.run(['sh', str(root/'run.sh'), '--display', 'headless'], env=env, capture_output=True, text=True)
    assert result.returncode == 78 and 'Missing per-device configuration' in result.stderr
print('Application archive layout, exclusions, checksums and fail-closed launcher passed; no ARM execution.')
