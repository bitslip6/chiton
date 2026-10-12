"""Retain simulator logs for CI without retaining fixtures, plugins or calibration."""
from contextlib import contextmanager
import fcntl
import os
from pathlib import Path
import shutil
import tempfile


@contextmanager
def runtime_directory(prefix):
    # The application still shares ports 80/8080 (F08). RUN_SERIAL only applies
    # within one CTest invocation; coordinate concurrent local gate variants too.
    lock_path = Path(tempfile.gettempdir()) / f'chiton-http-tests-{os.getuid()}.lock'
    with open(lock_path, 'a') as lock, tempfile.TemporaryDirectory(prefix=prefix) as directory:
        fcntl.flock(lock, fcntl.LOCK_EX)
        try:
            yield directory
        finally:
            if os.environ.get('CHITON_TEST_LOG_DIR'):
                target = Path(os.environ['CHITON_TEST_LOG_DIR']) / Path(directory).name
                target.mkdir(parents=True, exist_ok=True)
                for log in Path(directory).glob('*.log'):
                    shutil.copyfile(log, target / log.name)
            forbidden = Path(directory) / 'forbidden-commands.log'
            assert not forbidden.exists(), 'Simulator attempted a host command: ' + forbidden.read_text()


def simulator_environment(directory):
    """Tripwires: even an ignored subprocess error cannot become a passing test."""
    env = os.environ.copy()
    env['ADITOF_STUB_PAUSE_FILE'] = str(Path(directory) / 'capture.pause')
    env['ADITOF_STUB_LIFECYCLE_FAILURE_FILE'] = str(Path(directory) / 'lifecycle.fail')
    guards = Path(directory) / 'command-guards'
    guards.mkdir()
    for command in ('sudo', 'reboot', 'shutdown', 'nmcli', 'ip', 'route', 'systemctl', 'iw', 'i2cset', 'i2cget'):
        guard = guards / command
        guard.write_text('#!/bin/sh\nprintf "%s\\n" "$0 $*" >> "$CHITON_FORBIDDEN_COMMAND_LOG"\nexit 99\n')
        guard.chmod(0o700)
    env['PATH'] = str(guards) + os.pathsep + env.get('PATH', os.defpath)
    env['CHITON_FORBIDDEN_COMMAND_LOG'] = str(Path(directory) / 'forbidden-commands.log')
    return env
