#!/usr/bin/env python3
"""Compile-only old-header regression probe; never links or executes ARM code.

Requires an explicitly supplied recovered Yocto compile_commands.json. Missing
prerequisites are errors, not a host-header fallback or a successful skip.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-commands', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True,
                        help='Ignored evidence directory for command/object output')
    args = parser.parse_args()
    entries = json.loads(args.compile_commands.read_text())
    entry = next(e for e in entries if e['file'].endswith('/src/webserver/webserver.cpp'))
    command = entry.get('arguments') or shlex.split(entry['command'])
    if '-nostdinc' not in command or '-nostdinc++' not in command:
        parser.error('Expected recovered-header flags -nostdinc and -nostdinc++')
    if not any('/include/c++/7.3.0' in flag for flag in command):
        parser.error('Expected recovered GCC 7.3.0 headers')
    triple = subprocess.check_output([command[0], '-dumpmachine'], text=True).strip()
    if not triple.startswith('aarch64'):
        parser.error('Expected AArch64 compiler, got ' + triple)
    source = Path(__file__).resolve().with_name('bounded_http.cpp')
    args.output.mkdir(parents=True, exist_ok=True)
    output = (args.output / 'bounded_http.o').resolve()
    output.unlink(missing_ok=True)  # a failed probe must not leave a stale object
    result = []
    i = 0
    while i < len(command):
        if command[i] == '-o':
            result += ['-o', str(output)]
            i += 2
        elif command[i] == entry['file']:
            result.append(str(source))
            i += 1
        else:
            result.append(command[i])
            i += 1
    if '-c' not in result or str(source) not in result or str(output) not in result:
        parser.error('Unsupported compile database entry: expected source, -c and -o')
    (args.output / 'command.txt').write_text(shlex.join(result) + '\n')
    print(shlex.join(result), flush=True)
    subprocess.run(result, cwd=entry['directory'], check=True)
    print('PASS: GCC 7.3-header AArch64 compile-only probe (object not executed)')


if __name__ == '__main__':
    main()
