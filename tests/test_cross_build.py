#!/usr/bin/env python3
"""Host-only mocked packaging/launcher tests. No CMake, compiler or ARM execution."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('packager', ROOT/'tools/package_aarch64.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class CrossBuildTests(unittest.TestCase):
    def test_wrapper_forwarding(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            fake = folder/'python3'
            fake.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$CAPTURE"\n')
            fake.chmod(0o755)
            capture = folder/'args'
            env = dict(os.environ, PATH=str(folder)+os.pathsep+os.environ['PATH'], CAPTURE=str(capture))
            subprocess.run(['bash', str(ROOT/'cross-build.sh'), '--workspace', '/workspace with spaces',
                            '--name', 'rel_test'], check=True, env=env)
            self.assertEqual(capture.read_text().splitlines(), [str(ROOT/'tools/package_aarch64.py'),
                             '--include-benchmarks', '--workspace', '/workspace with spaces', '--name', 'rel_test'])
            for args in (['--unknown'], ['--name'], ['--name', '--output']):
                result = subprocess.run(['bash', str(ROOT/'cross-build.sh'), *args], env=env, capture_output=True)
                self.assertEqual(result.returncode, 2)
            result = subprocess.run(['bash', str(ROOT/'cross-build.sh')],
                                    env=dict(env, JOBS='0'), capture_output=True)
            self.assertEqual(result.returncode, 2)

    def test_simd_options_do_not_leak_from_previous_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace=Path(directory); (workspace/'build').mkdir()
            cache=('CMAKE_COMMAND:INTERNAL=/tools/cmake\nCHITON_SIMD_GREYSCALE:BOOL=ON\n'
                   'CHITON_SIMD_CALIBRATION:BOOL=ON\nCHITON_EXPERIMENTAL_ILLUMINATION:BOOL=OFF\n'
                   'CHITON_DEFAULT_DISPLAY:STRING=framebuffer\n')
            (workspace/'build/CMakeCache.txt').write_text(cache)
            with patch.object(packager.subprocess, 'run') as run:
                packager.configure_simd(workspace,cache,False,False)
                command=run.call_args_list[0].args[0]
                self.assertIn('-DCHITON_SIMD_GREYSCALE=OFF',command)
                self.assertIn('-DCHITON_SIMD_CALIBRATION=OFF',command)
                self.assertIn('-DCHITON_EXPERIMENTAL_ILLUMINATION=OFF',command)
                self.assertIn('-DCHITON_DEFAULT_DISPLAY=framebuffer',command)
                self.assertEqual(run.call_args_list[1].args[0][4],'chiton')
            with patch.object(packager.subprocess, 'run') as run:
                packager.configure_simd(workspace,cache,True,True)
                run.assert_not_called()

    def test_illumination_option_does_not_leak(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory); (workspace/'build').mkdir()
            cache = ('CMAKE_COMMAND:INTERNAL=/tools/cmake\nCHITON_SIMD_GREYSCALE:BOOL=OFF\n'
                     'CHITON_SIMD_CALIBRATION:BOOL=OFF\nCHITON_EXPERIMENTAL_ILLUMINATION:BOOL=ON\n'
                     'CHITON_DEFAULT_DISPLAY:STRING=framebuffer\n')
            (workspace/'build/CMakeCache.txt').write_text(cache)
            with patch.object(packager.subprocess, 'run') as run:
                packager.configure_simd(workspace, cache, False, False)
                self.assertIn('-DCHITON_EXPERIMENTAL_ILLUMINATION=OFF', run.call_args_list[0].args[0])
                self.assertIn('-DCHITON_DEFAULT_DISPLAY=framebuffer', run.call_args_list[0].args[0])
            with patch.object(packager.subprocess, 'run') as run:
                packager.configure_simd(workspace, cache, False, False, True)
                run.assert_not_called()
            fake = workspace/'python3'
            fake.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$CAPTURE"\n')
            fake.chmod(0o755)
            capture = workspace/'args'
            env = dict(os.environ, PATH=str(workspace)+os.pathsep+os.environ['PATH'], CAPTURE=str(capture))
            subprocess.run(['bash', str(ROOT/'cross-build.sh'), '--experimental-illumination'], check=True, env=env)
            self.assertEqual(capture.read_text().splitlines(),
                             [str(ROOT/'tools/package_aarch64.py'), '--include-benchmarks', '--experimental-illumination'])

    def test_benchmark_build_is_explicit_and_not_executed(self):
        with patch.object(packager.subprocess, 'run') as run:
            programs = packager.build_benchmarks(Path('/workspace'), 'CMAKE_COMMAND:INTERNAL=/tools/cmake\n')
            self.assertEqual(set(programs), set(packager.BENCHMARKS))
            calls = [call.args[0] for call in run.call_args_list]
            self.assertIn('-DBUILD_PIPELINE_BENCHMARKS=ON', calls[0])
            self.assertIn('--target', calls[1])
            self.assertEqual(calls[1][calls[1].index('--target')+1:][:len(packager.BENCHMARKS)], list(packager.BENCHMARKS))
            self.assertTrue(all(command[0] == '/tools/cmake' for command in calls))

    def test_strong_symbols_and_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            (workspace/'build').mkdir(); (workspace/'sysroot').mkdir()
            program = workspace/'build'/packager.BENCHMARKS[0]
            library = workspace/'sysroot/libc.so.6'
            program.touch(); library.touch()
            link_map = program.with_name(program.name+'.map')
            link_map.write_text(f'LOAD {program}\nLOAD {library}\n')
            def symbols(path, *args):
                return ('  1: 0 0 FUNC GLOBAL DEFAULT UND foo@GLIBC_2.27\n' if path == program else
                        '  1: 0 0 FUNC GLOBAL DEFAULT 12 foo@@GLIBC_2.27\n')
            with patch.object(packager, 'elf', side_effect=symbols):
                packager.audit_benchmark(program, {program.name:program, library.name:library}, workspace)
                link_map.write_text('LOAD /usr/lib/distribution-libgcc.a\n')
                with self.assertRaises(ValueError):
                    packager.audit_benchmark(program, {program.name:program, library.name:library}, workspace)
            link_map.write_text(f'LOAD {program}\n')
            with patch.object(packager, 'elf', return_value='  1: 0 0 FUNC GLOBAL DEFAULT UND missing\n'):
                with self.assertRaises(ValueError):
                    packager.audit_benchmark(program, {program.name:program}, workspace)

    def test_target_launchers_with_host_stubs_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            diagnostics = root/'benchmarks'; diagnostics.mkdir(); (root/'lib').mkdir()
            for script in ('run.sh', 'run-all.sh', 'run-display-sweep.sh'):
                shutil.copy2(ROOT/'tools/benchmark-tools'/script, diagnostics/script)
                (diagnostics/script).chmod(0o755)
            for name in packager.BENCHMARKS:
                stub = diagnostics/name
                stub.write_text('#!/bin/sh\nprintf "%s|%s\\n" "$LD_LIBRARY_PATH" "$*"\n')
                stub.chmod(0o755)
            result = subprocess.run(['sh', str(diagnostics/'run-all.sh'), '1'], check=True,
                                    capture_output=True, text=True)
            lines = result.stdout.splitlines()
            self.assertEqual(len(lines), 11)
            self.assertTrue(all(line.startswith(str(root/'lib')+'|') or
                                line.startswith(str(diagnostics/'../lib')+'|') for line in lines))
            self.assertTrue(lines[0].endswith('grey16 1'))
            self.assertTrue(lines[-1].endswith('calibrate 1 1'))
            result = subprocess.run(['sh', str(diagnostics/'run.sh'), 'display', '--help'],
                                    check=True, capture_output=True, text=True)
            self.assertTrue(result.stdout.strip().endswith('|--help'))
            result = subprocess.run(['sh', str(diagnostics/'run.sh'), 'spi', '--help'],
                                    check=True, capture_output=True, text=True)
            self.assertTrue(result.stdout.strip().endswith('|--help'))
            result = subprocess.run(['sh', str(diagnostics/'run-display-sweep.sh'), '--seconds', '1'],
                                    check=True, capture_output=True, text=True)
            sweep = result.stdout.splitlines()
            self.assertEqual(len(sweep), 7)
            self.assertTrue(sweep[0].endswith('--seconds 1 --region 320x240 --delay-ms 110'))
            self.assertTrue(sweep[-1].endswith('--seconds 1 --region 320x24 --delay-ms 0'))
            for value in ('0', '1000001', 'no', '99999999999999999999'):
                result = subprocess.run(['sh', str(diagnostics/'run-all.sh'), value], capture_output=True)
                self.assertEqual(result.returncode, 2)


if __name__ == '__main__':
    unittest.main()
