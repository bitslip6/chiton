import importlib.util
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "generate_stub_images.py"
spec = importlib.util.spec_from_file_location("generate_stub_images", SCRIPT)
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class StubImageGeneratorTests(unittest.TestCase):
    def pixel(self, image, x, y):
        return struct.unpack_from("<H", image, (y * generator.WIDTH + x) * 2)[0]

    def cli(self, directory, *args):
        return subprocess.run([sys.executable, str(SCRIPT), "--output-dir", str(directory), *args],
                              capture_output=True, text=True)

    def test_defaults_and_little_endian_storage(self):
        for channel in ("ir", "depth"):
            image = generator.generate_image(channel)
            self.assertEqual(image, b"\x40\x01" * (640 * 480))
            maximum = generator.generate_image(channel, value=4095)
            self.assertEqual(maximum[:2], b"\xff\x0f")
            self.assertEqual(len(maximum), 614400)

    def test_patterns_stay_12_bit_and_have_expected_orientation(self):
        for pattern in ("gradient", "checkerboard"):
            for channel in ("ir", "depth"):
                image = generator.generate_image(channel, pattern)
                self.assertEqual(len(image), 614400)
                values = struct.unpack("<307200H", image)
                self.assertEqual(min(values), 0)
                self.assertEqual(max(values), 4095)
                self.assertTrue(all(value <= 4095 for value in values))
                if pattern == "gradient":
                    self.assertEqual(self.pixel(image, 0, 0), 0)
                    self.assertEqual(self.pixel(image, 639, 479), 4095)
                    self.assertEqual(self.pixel(image, 639, 0), 4095 if channel == "ir" else 0)
                    self.assertEqual(self.pixel(image, 0, 479), 0 if channel == "ir" else 4095)
                else:
                    self.assertEqual(self.pixel(image, 0, 0), 0 if channel == "ir" else 4095)
                    self.assertEqual(self.pixel(image, 40, 0), 4095 if channel == "ir" else 0)

    def test_cli_output_and_safe_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.cli(directory, "--ir-value", "0", "--depth-value", "0xFFF")
            self.assertEqual(result.returncode, 0, result.stderr)
            ir = Path(directory) / "aditof-ir.bin"
            depth = Path(directory) / "aditof-depth.bin"
            self.assertEqual(ir.read_bytes(), b"\x00\x00" * (640 * 480))
            self.assertEqual(depth.read_bytes(), b"\xff\x0f" * (640 * 480))
            ir.unlink()
            # An existing second file prevents writing even the missing first file.
            result = self.cli(directory)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("--force", result.stderr)
            self.assertFalse(ir.exists())
            self.assertEqual(depth.read_bytes()[:2], b"\xff\x0f")
            result = self.cli(directory, "--force")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(ir.read_bytes(), depth.read_bytes())
            self.assertEqual(ir.read_bytes()[:2], b"\x40\x01")

    def test_rejects_invalid_samples_without_writing(self):
        for value in ("-1", "4096", "65535", "invalid"):
            with tempfile.TemporaryDirectory() as directory:
                result = self.cli(directory, "--ir-value", value)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(list(Path(directory).iterdir()), [])
        for value in (-1, 4096, 65535):
            with self.assertRaises(ValueError):
                generator.generate_image("ir", value=value)


if __name__ == "__main__":
    unittest.main()
