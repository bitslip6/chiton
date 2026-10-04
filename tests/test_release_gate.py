#!/usr/bin/env python3
"""Gate wiring must fail, not skip, on tool/test/sanitizer/host-command errors."""
from contextlib import redirect_stdout
import io
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import release_gate
from runtime_support import runtime_directory, simulator_environment


class ReleaseGateTests(unittest.TestCase):
    def invoke(self, output, *options, code=0, diagnostic=''):
        calls = []

        def command(argv, **kwargs):
            calls.append((argv, kwargs['env'].copy()))
            kwargs['stdout'].write(diagnostic)
            return subprocess.CompletedProcess(argv, code)

        with patch('sys.argv', ['release_gate', '--output', str(output), *options]), \
             patch('release_gate.subprocess.run', side_effect=command), redirect_stdout(io.StringIO()):
            release_gate.main()
        return calls

    def test_simulator_is_explicit_and_environment_isolated(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, {
                'CHITON_ASSET_DIR': '/unsafe', 'ADITOF_STUB_DEPTH': '/unsafe',
                'LD_PRELOAD': '/unsafe', 'CXXFLAGS': '-ffast-math'}):
            calls = self.invoke(Path(directory) / 'gate', '--sanitizer', 'thread')
            configure = calls[0][0]
            self.assertIn('-DUSE_ADITOF_STUB=ON', configure)
            self.assertIn('-DUSE_SDL2=OFF', configure)
            self.assertIn('-DCHITON_RELEASE_TESTS=ON', configure)
            self.assertIn('-DCHITON_SANITIZER=thread', configure)
            for _, env in calls:
                self.assertNotIn('LD_PRELOAD', env)
                self.assertNotIn('ADITOF_STUB_DEPTH', env)
                self.assertNotIn('CHITON_ASSET_DIR', env)
                self.assertNotIn('CXXFLAGS', env)
                self.assertEqual(env['TSAN_OPTIONS'], 'halt_on_error=1')
            self.assertEqual(calls[-1][0][0], 'ctest')

    def test_nonzero_command_and_sanitizer_diagnostic_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, 'configure failed'):
                self.invoke(Path(directory) / 'exit', code=1)
            self.assertTrue((Path(directory) / 'exit/logs/configure.log').exists())
            with self.assertRaisesRegex(RuntimeError, 'Sanitizer diagnostic'):
                self.invoke(Path(directory) / 'diagnostic', diagnostic='WARNING: ThreadSanitizer: data race\n')

    def test_reusing_build_tree_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(FileExistsError):
                self.invoke(Path(directory))

    def test_browser_required_and_both_copies_tested(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, {}, clear=True):
            with patch('release_gate.shutil.which', return_value=None):
                with self.assertRaisesRegex(RuntimeError, 'Chromium is required'):
                    self.invoke(Path(directory) / 'missing', '--browser-only')
            with patch('release_gate.shutil.which', return_value='/mock/chromium'):
                calls = self.invoke(Path(directory) / 'present', '--browser-only')
            self.assertEqual(len(calls), 3)
            self.assertEqual(calls[1][1]['CHITON_ASSET_DIR'], str(release_gate.ROOT / 'release/files'))
            self.assertEqual(calls[2][1]['CHITON_ASSET_DIR'], str(release_gate.ROOT / 'bin/files'))

    def test_packaging_requires_every_variant_and_browser(self):
        with tempfile.TemporaryDirectory() as directory:
            calls = self.invoke(Path(directory) / 'all', '--all')
            self.assertEqual(len(calls), 5)
            for call, variant in zip(calls[:4], ('none', 'address', 'undefined', 'thread')):
                self.assertIn(variant, call[0])
            self.assertIn('--browser-only', calls[-1][0])

    def test_packaging_stops_before_copy_on_gate_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / 'mkrelease.sh'
            script.write_text((release_gate.ROOT / 'mkrelease.sh').read_text())
            (root / 'bin').mkdir(); (root / 'release').mkdir(); (root / 'tools').mkdir()
            (root / 'bin/chiton').write_bytes(b'new binary (never executed)')
            target = root / 'release/chiton'
            target.write_bytes(b'previous release')
            python = root / 'tools/python3'
            python.write_text('#!/bin/sh\nexit 1\n'); python.chmod(0o700)
            env = os.environ.copy(); env['PATH'] = str(root / 'tools') + os.pathsep + os.defpath
            result = subprocess.run(['/bin/sh', str(script)], env=env)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(target.read_bytes(), b'previous release')
            python.write_text('#!/bin/sh\ntest "$*" = "tests/release_gate.py --all"\n')
            self.assertEqual(subprocess.run(['/bin/sh', str(script)], env=env).returncode, 0)
            self.assertEqual(target.read_bytes(), (root / 'bin/chiton').read_bytes())

    def test_host_command_attempt_is_blocked_and_fails(self):
        # This sudo is our disposable tripwire, not the host executable.
        with self.assertRaisesRegex(AssertionError, 'Simulator attempted a host command'):
            with runtime_directory('chiton-gate-tripwire-') as directory:
                env = simulator_environment(directory)
                self.assertEqual(subprocess.run(['sudo', 'reboot'], env=env).returncode, 99)


if __name__ == '__main__':
    unittest.main()
