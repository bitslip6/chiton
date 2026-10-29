#!/usr/bin/env python3
"""Host-only mocked packaging/launcher tests. No CMake, compiler or ARM execution."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('packager', ROOT/'tools/package_aarch64.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class CrossBuildTests(unittest.TestCase):
    def test_digest_without_python_311_api(self):
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(hashlib, 'file_digest', None, create=True):
            path = Path(directory) / 'payload'
            for payload in (b'', b'abc', bytes(range(256)) * 8193):
                path.write_bytes(payload)
                self.assertEqual(packager.digest(path), hashlib.sha256(payload).hexdigest())

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
            forwarded = capture.read_bytes()
            help_result = subprocess.run(['bash', str(ROOT/'cross-build.sh'), '--help'],
                                         check=True, env=env, capture_output=True, text=True)
            self.assertIn('tft_device.sh', help_result.stdout)
            self.assertIn('mkgpio.sh', help_result.stdout)
            self.assertEqual(capture.read_bytes(), forwarded, 'Help must not invoke the packager')
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

    def test_root_scripts_are_discovered_copied_and_never_run(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root/'source'; target = root/'bundle'
            source.mkdir(); target.mkdir(); (source/'nested').mkdir()
            for name, mode in [('board.sh', 0o755), ('host build.sh', 0o640)]:
                script = source/name
                script.write_text(f'#!/bin/sh\ntouch "{root}/MUST_NOT_RUN"\n')
                script.chmod(mode)
            (source/'nested/ignored.sh').write_text('#!/bin/sh\n')
            (source/'ignored.txt').write_text('not a shell script')
            scripts = packager.copy_root_scripts(source, target)
            self.assertEqual(set(scripts), {'board.sh', 'host build.sh'})
            self.assertFalse((root/'MUST_NOT_RUN').exists())
            for name, checksum in scripts.items():
                self.assertEqual((target/name).read_bytes(), (source/name).read_bytes())
                self.assertEqual((target/name).stat().st_mode & 0o777, (source/name).stat().st_mode & 0o777)
                self.assertEqual(checksum, packager.digest(source/name))
            # New root scripts are picked up without updating a hard-coded allowlist.
            (source/'new.sh').write_text('#!/bin/sh\n')
            fresh = root/'fresh'; fresh.mkdir()
            self.assertEqual(set(packager.copy_root_scripts(source, fresh)), set(scripts) | {'new.sh'})

    def test_root_scripts_protect_launchers_and_reject_escaping_symlinks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root/'source'; target = root/'bundle'
            source.mkdir(); target.mkdir()
            for name in ('run.sh', 'chiton.sh', 'existing.sh'):
                (source/name).write_text('must not replace bundle entry')
                (target/name).write_text('keep')
                with self.assertRaises(ValueError):
                    packager.copy_root_scripts(source, target)
                self.assertEqual((target/name).read_text(), 'keep')
                (source/name).unlink()
            outside = root/'outside.sh'; outside.write_text('not in source')
            (source/'escape.sh').symlink_to(outside)
            with self.assertRaises(ValueError):
                packager.copy_root_scripts(source, target)
            (source/'escape.sh').unlink()
            (source/'broken.sh').symlink_to(source/'missing')
            with self.assertRaises(FileNotFoundError):
                packager.copy_root_scripts(source, target)

    def test_required_board_scripts_are_copied_from_their_exact_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)
            roots, board = packager.copy_bundle_scripts(ROOT, target)
            self.assertEqual(set(board), {'mkgpio.sh', 'tft_device.sh'})
            self.assertEqual(roots['mkgpio.sh'], board['mkgpio.sh'])
            self.assertNotIn('tft_device.sh', roots)
            for name, source in {'mkgpio.sh': ROOT/'mkgpio.sh',
                                 'tft_device.sh': ROOT/'release/tft_device.sh'}.items():
                self.assertEqual((target/name).read_bytes(), source.read_bytes())
                self.assertEqual((target/name).stat().st_mode & 0o777, source.stat().st_mode & 0o777)
                self.assertEqual(packager.digest(target/name), board[name])
                subprocess.run(['sh', '-n', str(target/name)], check=True)
            self.assertFalse((target/'setup.sh').exists())
            self.assertFalse((target/'rc.local').exists())

    def test_required_board_scripts_fail_closed_before_copying(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root/'source'; target = root/'bundle'
            source.mkdir(); target.mkdir(); (source/'release').mkdir()
            scripts = [source/'mkgpio.sh', source/'release/tft_device.sh']
            for script in scripts:
                script.write_text(f'#!/bin/sh\ntouch "{root}/MUST_NOT_RUN"\n')
                script.chmod(0o755)
            for script in scripts:
                saved = script.read_text(); script.unlink()
                with self.assertRaises(FileNotFoundError):
                    packager.copy_bundle_scripts(source, target)
                self.assertEqual(list(target.iterdir()), [])
                script.write_text(saved); script.chmod(0o644)
                with self.assertRaises(ValueError):
                    packager.copy_bundle_scripts(source, target)
                self.assertEqual(list(target.iterdir()), [])
                script.chmod(0o755)
            for conflict in [source/'tft_device.sh', target/'tft_device.sh', target/'mkgpio.sh']:
                conflict.write_text('keep')
                with self.assertRaises(ValueError):
                    packager.copy_bundle_scripts(source, target)
                self.assertEqual(conflict.read_text(), 'keep'); conflict.unlink()
            outside = root/'outside.sh'; outside.write_text('#!/bin/sh\n'); outside.chmod(0o755)
            scripts[1].unlink(); scripts[1].symlink_to(outside)
            with self.assertRaises(ValueError):
                packager.copy_bundle_scripts(source, target)
            self.assertFalse((root/'MUST_NOT_RUN').exists())

    def test_archive_contains_required_board_hooks_and_checksums_without_execution(self):
        # Mock the compiler/auditor only. Build/extract the real tar layout and
        # run the standalone host validator; neither board hooks nor ARM run.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root/'source'; workspace = root/'workspace'; output = root/'dist'
            for path in ['release/files', 'release/jabil', 'docs']:
                (source/path).mkdir(parents=True)
            for name in ['app.html', 'debug.html', 'large_font.png', 'small_font.png', 'image-stream.js']:
                (source/'release/files'/name).write_text('fixture asset')
            (source/'release/chiton.json').write_text('{}')
            (source/'docs/illumination-controller-integration.md').write_text('fixture documentation')
            for path in ['mkgpio.sh', 'release/tft_device.sh']:
                script = source/path
                script.write_text(f'#!/bin/sh\ntouch "{root}/MUST_NOT_RUN"\n'); script.chmod(0o755)
            (workspace/'build').mkdir(parents=True); (workspace/'stage/lib').mkdir(parents=True)
            binary = workspace/'build/chiton'; binary.write_bytes(b'\x7fELFnot-executable')
            library = workspace/'stage/lib/libaditof.so.1.0'; library.write_bytes(b'\x7fELFnot-executable')
            cache = (f'CMAKE_HOME_DIRECTORY:INTERNAL={source}\nUSE_ADITOF_STUB:BOOL=OFF\n'
                     'USE_SDL2:BOOL=OFF\n')
            (workspace/'build/CMakeCache.txt').write_text(cache)
            args = ['package_aarch64.py', '--workspace', str(workspace), '--output', str(output), '--name', 'rel_board_test']
            with patch.object(packager, 'ROOT', source), patch('sys.argv', args), \
                 patch.object(packager.subprocess, 'run') as commands, \
                 patch.object(packager, 'configure_simd', return_value=cache), \
                 patch.object(packager, 'closure', return_value={'chiton': binary, library.name: library}), \
                 patch.object(packager, 'elf', return_value='GLIBC_2.27'), \
                 patch.object(packager, 'run', return_value='test-build'):
                packager.main()
                self.assertTrue(all(command.args[0][0] in {'bash', 'python3', 'sh'} for command in commands.call_args_list))
                self.assertFalse(any('tft_device.sh' in str(command.args[0]) or 'mkgpio.sh' in str(command.args[0])
                                     for command in commands.call_args_list))
            archive = output/'rel_board_test.tar.gz'
            self.assertEqual((output/'rel_board_test.tar.gz.sha256').read_text().split()[0], packager.digest(archive))
            with tarfile.open(archive) as tar:
                manifest = json.load(tar.extractfile('rel_board_test/manifest.json'))
                sums = tar.extractfile('rel_board_test/SHA256SUMS').read().decode()
                for name in ['mkgpio.sh', 'tft_device.sh']:
                    entry = tar.getmember('rel_board_test/'+name)
                    self.assertTrue(entry.isfile()); self.assertEqual(entry.mode & 0o777, 0o755)
                    data = tar.extractfile(entry).read(); expected = hashlib.sha256(data).hexdigest()
                    source_path = source/('release/tft_device.sh' if name == 'tft_device.sh' else name)
                    self.assertEqual(data, source_path.read_bytes())
                    self.assertEqual(manifest['required_board_scripts'][name], expected)
                    self.assertIn(f'{expected}  {name}\n', sums)
            subprocess.run(['python3', str(ROOT/'tests/test_aarch64_package.py'), str(archive)], check=True)
            self.assertFalse((root/'MUST_NOT_RUN').exists())

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
