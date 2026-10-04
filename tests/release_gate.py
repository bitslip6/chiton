#!/usr/bin/env python3
"""Fail-closed, isolated desktop release checks; never uses bin/chiton or hardware."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizer', choices=('none', 'address', 'undefined', 'thread'), default='none')
    parser.add_argument('--output', type=Path, help='new output directory (retained on failure and success)')
    parser.add_argument('--jobs', type=int, default=2)
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument('--browser-only', action='store_true', help='require Chromium; test both deployed copies')
    scope.add_argument('--all', action='store_true', help='require all four simulator variants and Chromium')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if (args.all or args.browser_only) and args.sanitizer != 'none':
        parser.error('--sanitizer is only valid for a single simulator gate')
    out = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='chiton-release-'))
    if args.output:
        # Refuse reused trees, including symlinks, so build flags cannot mix.
        out.mkdir(parents=True, exist_ok=False)
    logs = out / 'logs'
    logs.mkdir()
    env = os.environ.copy()
    # Avoid inherited user plugin/SDK/configuration and sanitizer overrides.
    for key in list(env):
        if key.startswith(('CHITON_', 'ADITOF_STUB_')) or key in (
                'LD_PRELOAD', 'ASAN_OPTIONS', 'UBSAN_OPTIONS', 'TSAN_OPTIONS',
                'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'CMAKE_TOOLCHAIN_FILE'):
            env.pop(key)
    if os.environ.get('CHITON_CHROMIUM'):
        env['CHITON_CHROMIUM'] = os.environ['CHITON_CHROMIUM']
    env.update(PYTHONDONTWRITEBYTECODE='1', CHITON_TEST_LOG_DIR=str(logs / 'runtime'),
               ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1', TSAN_OPTIONS='halt_on_error=1')
    print(f'Release gate artifacts: {out}', flush=True)

    def run(name, command, timeout=1200):
        print(f'[{name}] {" ".join(map(str, command))}', flush=True)
        with (logs / (name + '.log')).open('w') as log:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=timeout)
        if result.returncode:
            raise RuntimeError(f'{name} failed ({result.returncode}); see {logs / (name + ".log")}')

    if args.all:
        for variant in ('none', 'address', 'undefined', 'thread'):
            run('gate-' + variant, [sys.executable, str(Path(__file__).resolve()), '--sanitizer', variant,
                                   '--output', str(out / variant), '--jobs', str(args.jobs)], timeout=1800)
        run('gate-browser', [sys.executable, str(Path(__file__).resolve()), '--browser-only',
                             '--output', str(out / 'browser')], timeout=600)
    elif args.browser_only:
        chromium = env.get('CHITON_CHROMIUM') or shutil.which('chromium') or shutil.which('google-chrome')
        # CHITON_CHROMIUM is a tool selection, not a sensor/config override.
        chromium = os.environ.get('CHITON_CHROMIUM', chromium)
        if not chromium:
            raise RuntimeError('Chromium is required, not an optional skipped gate')
        env['CHITON_CHROMIUM'] = chromium
        run('web-ui', ['node', 'tests/web_ui.js'])
        for copy in ('release', 'bin'):
            env['CHITON_ASSET_DIR'] = str(ROOT / copy / 'files')
            run('chromium-' + copy, ['node', 'tests/web_stream_browser.js'], timeout=300)
    else:
        run('configure', ['cmake', '-S', str(ROOT), '-B', str(out / 'build'),
                          '-DUSE_ADITOF_STUB=ON', '-DUSE_SDL2=OFF', '-DCHITON_RELEASE_TESTS=ON',
                          '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_CXX_FLAGS_DEBUG=-O1 -g',
                          '-DCHITON_SANITIZER=' + args.sanitizer,
                          '-DCHITON_INSTALL_DIR=' + str(out / 'unused-install')])
        run('build', ['cmake', '--build', str(out / 'build'), '--parallel', str(args.jobs)])
        run('ctest', ['ctest', '--test-dir', str(out / 'build'), '--output-on-failure', '-j1'], timeout=1800)
    # Runtime logs outlive the suites' disposable fixtures, including restart
    # runs with a deliberately nonzero exit. No sanitizer diagnostics may hide.
    for log in logs.rglob('*.log'):
        if re.search(r'ERROR: (AddressSanitizer|LeakSanitizer)|WARNING: ThreadSanitizer|'
                     r'FATAL: ThreadSanitizer|runtime error:', log.read_text(errors='replace')):
            raise RuntimeError(f'Sanitizer diagnostic in {log}')
    print('Release gate passed', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
