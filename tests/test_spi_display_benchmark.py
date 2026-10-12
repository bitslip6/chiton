#!/usr/bin/env python3
"""Host-safe direct SPI benchmark tests; never access spidev/GPIO hardware."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DirectSPITests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='spi-display-benchmark-')
        cls.program = Path(cls.temp.name)/'spi_display_benchmark'
        subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-O2', '-Wall', '-Wextra',
                        '-Werror', '-pthread', str(ROOT/'tools/spi_display_benchmark.cpp'),
                        '-o', str(cls.program)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_program(self, *args):
        return subprocess.run([str(self.program), *args], capture_output=True, text=True, timeout=10)

    def test_help_and_self_test(self):
        result = self.run_program('--help')
        self.assertEqual(result.returncode, 0)
        self.assertIn('commands ALWAYS use 8 bits', result.stdout)
        result = self.run_program('--self-test')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('self-test passed', result.stdout)

    def test_requires_explicit_ownership_acknowledgement(self):
        for args in [(), ('--dc-gpio', '118'), ('--ack-exclusive',)]:
            result = self.run_program(*args)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('no device opened', result.stderr)

    def test_invalid_options(self):
        for args in [('--word-bits', '9'), ('--word-bits', '24'),
                     ('--word-bits', '16', '--chunk-bytes', '5'),
                     ('--word-bits', '32', '--chunk-bytes', '6'),
                     ('--chunk-bytes', '0'), ('--chunk-bytes', '153601'),
                     ('--speed-hz', '0'), ('--speed-hz', '62500001'),
                     ('--mode', '4'), ('--seconds', 'nan'), ('--seconds', 'inf'),
                     ('--frames', '2.5'), ('--frames', '1000001'), ('--delay-ms', '-1'),
                     ('--pattern', 'bad'), ('--dc-gpio', '-1'), ('--unknown', 'value')]:
            with self.subTest(args=args):
                result = self.run_program('--ack-exclusive', '--dc-gpio', '118', *args)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('open /dev/spidev', result.stderr)

    def test_initialization_guards(self):
        for args in [('--init-panel',), ('--reset-gpio', '121'),
                     ('--init-panel', '--reset-gpio', '118')]:
            result = self.run_program('--ack-exclusive', '--dc-gpio', '118', *args)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('open /dev/spidev', result.stderr)

    def test_spi_failure_precedes_gpio_access(self):
        result = self.run_program('--ack-exclusive', '--dc-gpio', '118',
                                  '--device', '/dev/null', '--word-bits', '32', '--chunk-bytes', '32768')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('read spidev settings', result.stderr)
        self.assertNotIn('GPIO', result.stderr)

    def test_init_profile_matches_repository_loader(self):
        source = (ROOT/'tools/spi_display_benchmark.cpp').read_text()
        profile = source.split('>>> profile = {', 1)[1].split('\n    };', 1)[0]
        values = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+', profile)]
        loader = (ROOT/'release/tft_device.sh').read_text().split('init=', 1)[1]
        expected = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+', loader)]
        self.assertEqual(expected[0], 0x11)  # Sleep-out is separately issued with its delay.
        self.assertEqual(values, expected[1:])


if __name__ == '__main__':
    unittest.main()
