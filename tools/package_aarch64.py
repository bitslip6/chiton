#!/usr/bin/env python3
"""Build a versioned application bundle; never install or execute ARM programs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
SYSTEM = {'libc.so.6', 'libm.so.6', 'libpthread.so.0', 'libdl.so.2',
          'libstdc++.so.6', 'libgcc_s.so.1', 'ld-linux-aarch64.so.1'}
BENCHMARKS = ('image_pipeline_benchmark', 'calibration_pipeline_benchmark')


def run(*args):
    return subprocess.check_output(list(map(str, args)), text=True)


def digest(path):
    with path.open('rb') as file:
        return hashlib.file_digest(file, 'sha256').hexdigest()


def elf(path, *args):
    return run('aarch64-linux-gnu-readelf', '-W', *args, path)


def closure(binary, roots):
    files, pending = {}, [binary]
    while pending:
        path = pending.pop()
        if path.name in files:
            continue
        if not re.search(r'Machine:\s+AArch64', elf(path, '-h')):
            raise ValueError(f'Not AArch64: {path}')
        dynamic = elf(path, '-d')
        if re.search(r'\((?:RPATH|RUNPATH)\)', dynamic):
            raise ValueError(f'Unexpected runtime search path: {path}')
        files[path.name] = path
        for name in re.findall(r'\(NEEDED\).*?\[(.*?)\]', dynamic):
            library = next((root/name for root in roots if (root/name).is_file()), None)
            if library is None:
                raise ValueError(f'Missing {name}, required by {path}')
            pending.append(library)
    return files


def audit_benchmark(binary, libraries, workspace):
    """Inspect metadata/link provenance; never load or execute target code."""
    roots = [workspace/'build', workspace/'stage', workspace/'sysroot']
    for path in libraries.values():
        resolved = path.resolve(strict=True)
        if not any(resolved.is_relative_to(root.resolve()) for root in roots):
            raise ValueError(f'Outside recovered runtime/build: {path} -> {resolved}')
    provided, required = set(), []
    for path in libraries.values():
        for line in elf(path, '--dyn-syms').splitlines():
            columns = line.split()
            if len(columns) < 8 or not columns[0].rstrip(':').isdigit():
                continue
            name = columns[7].replace('@@', '@')
            if columns[6] != 'UND' and columns[4] in ('GLOBAL', 'WEAK'):
                provided.update((name, name.split('@')[0]))
            elif columns[6] == 'UND' and columns[4] != 'WEAK':
                required.append((path.name, name))
    missing = [(path, name) for path, name in required if name not in provided]
    if missing:
        raise ValueError(f'Unresolved benchmark strong symbols: {missing}')
    link_map = workspace/'build'/(binary.name + '.map')
    loads = re.findall(r'^LOAD (.*)$', link_map.read_text(), re.M)
    if not loads:
        raise ValueError(f'No linker provenance in {link_map}')
    for entry in loads:
        path = Path(entry.split('(', 1)[0])
        resolved = (path if path.is_absolute() else workspace/'build'/path).resolve()
        if not any(resolved.is_relative_to(root.resolve()) for root in roots):
            raise ValueError(f'Benchmark runtime/link input outside workspace: {entry}')


def build_benchmarks(workspace, cache):
    cmake = next((line.split('=', 1)[1] for line in cache.splitlines()
                  if line.startswith('CMAKE_COMMAND:INTERNAL=')), None)
    if not cmake:
        raise ValueError('Cannot locate workspace CMake command')
    env = dict(os.environ, PKG_CONFIG_LIBDIR=str(workspace/'sysroot/usr/lib/pkgconfig'),
               PKG_CONFIG_SYSROOT_DIR=str(workspace/'sysroot'))
    subprocess.run([cmake, '-S', str(ROOT), '-B', str(workspace/'build'),
                    '-DBUILD_PIPELINE_BENCHMARKS=ON'], check=True, env=env)
    jobs = os.environ.get('JOBS', '2')
    if not jobs.isdecimal() or int(jobs) < 1:
        raise ValueError('JOBS must be a positive integer')
    subprocess.run([cmake, '--build', str(workspace/'build'), '--target', *BENCHMARKS,
                    '--parallel', jobs], check=True, env=env)
    return {name: workspace/'build'/name for name in BENCHMARKS}


def copy_assets(source, destination):
    legacy_pages = {'app2.html', 'app3.html'}
    for path in source.rglob('*'):
        if path.name in legacy_pages:
            continue
        if path.is_symlink():
            path.resolve(strict=True).relative_to(source.resolve())
        if path.is_file():
            with path.open('rb') as file:
                if file.read(4) == b'\x7fELF':
                    raise ValueError(f'Unexpected executable in assets: {path}')
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns(*legacy_pages))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, default=ROOT.parent/'chiton-cross/yocto-sd')
    parser.add_argument('--output', type=Path, default=ROOT.parent/'chiton-cross/dist')
    parser.add_argument('--name', default='rel_cross_' + datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S'))
    parser.add_argument('--include-benchmarks', action='store_true',
                        help='Cross-build, audit and include SDK-free diagnostic programs/tools')
    args = parser.parse_args()
    if not re.fullmatch(r'rel_[A-Za-z0-9][A-Za-z0-9_.-]*', args.name) or '..' in args.name:
        parser.error('Release name must start rel_ and contain only safe filename characters')
    workspace = args.workspace.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    archive = output/(args.name + '.tar.gz')
    checksum = output/(archive.name + '.sha256')
    if archive.exists() or checksum.exists():
        parser.error('Output already exists; choose a new release name')
    subprocess.run(['bash', str(workspace/'rebuild.sh')], check=True)
    cache = (workspace/'build/CMakeCache.txt').read_text()
    if f'CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}' not in cache.splitlines():
        raise ValueError('Workspace was built from a different source checkout')
    if 'USE_ADITOF_STUB:BOOL=OFF' not in cache.splitlines() or 'USE_SDL2:BOOL=OFF' not in cache.splitlines():
        raise ValueError('Expected hardware-camera, no-SDL cross build')
    binary = workspace/'build/chiton'
    roots = [workspace/'stage/lib', workspace/'sysroot/lib', workspace/'sysroot/usr/lib']
    libraries = closure(binary, roots)
    benchmarks = build_benchmarks(workspace, cache) if args.include_benchmarks else {}
    benchmark_manifest = {}
    for name, program in benchmarks.items():
        dependencies = closure(program, roots)
        audit_benchmark(program, dependencies, workspace)
        if any(dep.startswith('libaditof') for dep in dependencies):
            raise ValueError(f'Benchmark unexpectedly depends on camera SDK: {name}')
        for dep, path in dependencies.items():
            if dep == name:
                continue
            if dep in libraries and libraries[dep].resolve() != path.resolve():
                raise ValueError(f'Conflicting dependency {dep}')
            libraries[dep] = path
        benchmark_manifest[name] = {
            'path': 'benchmarks/' + name, 'sha256': digest(program),
            'symbol_versions': sorted(set(re.findall(r'(?:GLIBCXX|GLIBC|CXXABI)_[0-9.]+', elf(program, '-V')))),
            'link_map_sha256': digest(workspace/'build'/(name + '.map')),
            'camera_sdk': False,
        }
    # The rebuild helper audited strong symbols and linker-map provenance too.
    subprocess.run(['python3', str(workspace/'audit-runtime.py')], check=True)
    with tempfile.TemporaryDirectory(prefix='chiton-package-') as temporary:
        bundle = Path(temporary)/args.name
        (bundle/'lib').mkdir(parents=True)
        (bundle/'defaults').mkdir()
        shutil.copy2(binary, bundle/'chiton.bin')
        if benchmarks:
            diagnostics = bundle/'benchmarks'
            diagnostics.mkdir()
            for name, program in benchmarks.items():
                shutil.copy2(program, diagnostics/name)
                (diagnostics/name).chmod(0o755)
            for name in ('run.sh', 'run-all.sh', 'README.md'):
                shutil.copy2(ROOT/'tools/benchmark-tools'/name, diagnostics/name)
            (diagnostics/'run.sh').chmod(0o755)
            (diagnostics/'run-all.sh').chmod(0o755)
            (diagnostics/'pipeline-performance-metrics.md').write_text(
                (ROOT/'docs/pipeline-performance-metrics.md').read_text())
            for name in ('run.sh', 'run-all.sh'):
                subprocess.run(['sh', '-n', str(diagnostics/name)], check=True)
        copy_assets(ROOT/'release/files', bundle/'files')
        copy_assets(ROOT/'release/jabil', bundle/'jabil')
        (bundle/'app.html').symlink_to('files/app.html')
        shutil.copy2(ROOT/'release/chiton.json', bundle/'defaults/chiton.example.json')
        for name, library in sorted(libraries.items()):
            if name != 'chiton' and name not in SYSTEM:
                # Store a regular file at its exact required SONAME; never ship
                # sysroot symlinks that resolve outside the release on-device.
                shutil.copyfile(library, bundle/'lib'/name)
                (bundle/'lib'/name).chmod(0o644)
        launcher = '''#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
STATE=${CHITON_STATE_DIR:-"$HERE/../shared"}
if [ ! -f "$STATE/chiton.json" ]; then
    echo "Missing per-device configuration: $STATE/chiton.json" >&2
    echo "Preserve the existing device configuration before starting this release." >&2
    exit 78
fi
STATE=$(CDPATH= cd -- "$STATE" && pwd -P)
export CHITON_RESOURCE_DIR="$HERE"
export CHITON_ASSET_DIR="$HERE/files"
export CHITON_CONFIG_FILE="$STATE/chiton.json"
export CHITON_CALIBRATION_FILE="$STATE/calibration.bin"
export LD_LIBRARY_PATH="$HERE/lib"
cd "$STATE"
exec "$HERE/chiton.bin" "$@"
'''
        (bundle/'run.sh').write_text(launcher)
        (bundle/'run.sh').chmod(0o755)
        # Preserve legacy executable/start-script names, but always enter the
        # environment-setting launcher rather than bypassing private libraries.
        (bundle/'chiton').symlink_to('run.sh')
        (bundle/'chiton.sh').symlink_to('run.sh')
        (bundle/'DEPLOY.txt').write_text(
            'Application update only, not a bootable Yocto image.\n'
            'Requires the recovered Yocto SD image runtime (glibc 2.27 / libstdc++ GCC 7.3).\n'
            'Extract under /usr/local/chiton as a new versioned release, never over current.\n'
            'Verify SHA256SUMS, preserve current/chiton.json and calibration.bin in ../shared.\n'
            'Do not replace libc, kernel, bootloader, drivers, networking or service configuration.\n'
            'Stop the existing Chiton instance before a manual run.sh --display headless test.\n'
            'Use files/debug.html for the initial image transport test; close other UI tabs.\n'
            'Activation via current symlink is a separate operator step after testing.\n'
            'Optional api.so is deliberately omitted: its source is absent and a fresh ABI build\n'
            'cannot be produced. Do not silently carry an old plugin into this release.\n')
        versions = sorted(set(re.findall(r'(?:GLIBCXX|GLIBC|CXXABI)_[0-9.]+', elf(binary, '-V'))))
        manifest = {
            'format': 'chiton-application-bundle-v1', 'release': args.name,
            'architecture': 'aarch64', 'cpu_baseline': 'armv8-a',
            'camera': 'hardware', 'display': 'framebuffer/headless; no SDL',
            'image_transport': 'chiton.jpeg.v1 WebSocket; snapshot fallback; 30 FPS hardware qualification pending',
            'compiler': run('aarch64-linux-gnu-g++', '-dumpfullversion').strip(),
            'source_commit': run('git', '-C', ROOT, 'rev-parse', 'HEAD').strip(),
            'source_status': run('git', '-C', ROOT, 'status', '--short').splitlines(),
            'binary_sha256': digest(binary), 'binary_symbol_versions': versions,
            'benchmarks': benchmark_manifest,
            'bundled_libraries': {name: digest(path) for name, path in sorted(libraries.items()) if name != 'chiton' and name not in SYSTEM},
            'required_system_libraries': {name: digest(path) for name, path in sorted(libraries.items()) if name in SYSTEM},
            'config_policy': 'External per-device shared state; example defaults are not installed automatically',
            'excluded': ['api.so (source absent)', 'glibc/loader/libstdc++/libgcc', 'legacy prebuilt programs/alternate UI pages', 'OS/network/provisioning scripts'],
            'hardware_image_qualification': 'not complete',
        }
        (bundle/'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        with (bundle/'SHA256SUMS').open('w') as file:
            for path in sorted(bundle.rglob('*')):
                if path.is_file() and not path.is_symlink() and path.name != 'SHA256SUMS':
                    file.write(f'{digest(path)}  {path.relative_to(bundle)}\n')
        subprocess.run(['sh', '-n', str(bundle/'run.sh')], check=True)
        with tarfile.open(archive, 'w:gz') as tar:
            tar.add(bundle, arcname=args.name)
    checksum.write_text(f'{digest(archive)}  {archive.name}\n')
    print(f'Application artifact: {archive}\nArchive checksum: {checksum}')
    print('No device files were changed; no ARM program or deployment script was executed.')


if __name__ == '__main__':
    main()
