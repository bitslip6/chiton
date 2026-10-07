#!/usr/bin/env python3
"""Host-safe CLI/pattern tests. No framebuffer or sysfs writes."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DisplayBenchmarkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='display-benchmark-test-')
        cls.program = Path(cls.temp.name)/'display_refresh_benchmark'
        subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-O2',
                        '-Wall', '-Wextra', '-Werror', '-pthread', '-I', str(ROOT/'include'),
                        str(ROOT/'tools/display_benchmark.cpp'),
                        str(ROOT/'src/graphics/display.cpp'), '-o', str(cls.program)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_program(self, *args):
        return subprocess.run([str(self.program), *args], capture_output=True, text=True, timeout=10)

    def test_help_and_self_test(self):
        result = self.run_program('--help')
        self.assertEqual(result.returncode, 0)
        self.assertIn('NOT measured panel FPS', result.stdout)
        result = self.run_program('--self-test')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('self-test passed', result.stdout)

    def test_invalid_options_fail_before_device_access(self):
        for args in [('--seconds', 'nan'), ('--seconds', 'inf'), ('--seconds', '0'),
                     ('--delay-ms', '-1'), ('--delay-ms', '5junk'),
                     ('--frames', '1.5'), ('--frames', '1000001'),
                     ('--region', '321x240'), ('--region', '320x0'), ('--region', '320'),
                     ('--method', 'bad'), ('--pattern', 'bad'), ('--unknown', '1'),
                     ('--seconds',), ('--spi-hz', '20000000'), ('--spi-path', '/sys/foo'),
                     ('--driver-fps', '30'), ('--fps-path', '/sys/foo'),
                     ('--self-test', '--spi-hz', '20000000', '--spi-path', '/sys/foo')]:
            with self.subTest(args=args):
                result = self.run_program(*args)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('open /dev/fb1', result.stderr)

    def test_valid_options_reach_ioctl_not_control_writes(self):
        result = self.run_program('--device', '/dev/null', '--seconds', '0.1',
                                  '--frames', '2', '--region', '1x1', '--method', 'pwrite',
                                  '--pattern', 'bars', '--delay-ms', '0.5', '--vsync',
                                  '--spi-hz', '20000000', '--spi-path', '/sys/does-not-exist')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('framebuffer info ioctl failed', result.stderr)
        self.assertNotIn('Applied', result.stderr)

    def test_missing_device(self):
        result = self.run_program('--device', '/nonexistent-display-benchmark')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('open /nonexistent-display-benchmark', result.stderr)


if __name__ == '__main__':
    unittest.main()
