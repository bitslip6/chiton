#!/usr/bin/env python3
"""Linux stub-camera integration checks. No root or graphical session needed.

Run with an SDL-enabled stub build. Ports 80/8080 must not be in use by another
service; the test only closes sockets/processes it owns.
"""
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time

binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else "bin/chiton").resolve())


def reserve(port):
    sock = socket.socket()
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("127.0.0.1", port))
        sock.listen()
        return sock
    except PermissionError:
        sock.close()
        return None
    except OSError:
        sock.close()
        raise RuntimeError(f"port {port} is already in use; stop that service before testing")


def request(port, path):
    with socket.create_connection(("127.0.0.1", port), timeout=2) as client:
        client.sendall(f"GET {path} HTTP/1.0\r\n\r\n".encode())
        chunks = []
        while True:
            chunk = client.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
        return b"".join(chunks)


with tempfile.TemporaryDirectory(prefix="chiton-runtime-") as directory:
    env = os.environ.copy()
    for variable in ("DISPLAY", "WAYLAND_DISPLAY", "SDL_VIDEODRIVER", "CHITON_DISPLAY"):
        env.pop(variable, None)
    env["CHITON_ASSET_DIR"] = directory + "/missing"
    # Configuration stays executable-relative; stub data stays in the temp dir.
    for variable, name in (("ADITOF_STUB_IR", "ir.bin"), ("ADITOF_STUB_DEPTH", "depth.bin")):
        env[variable] = directory + "/" + name
        Path(env[variable]).write_bytes(b"\x00\x00\xff\xff" * (640 * 480 // 2))

    def run(mode="auto", driver=None, blocked=False, expected_backend=None):
        current = env.copy()
        if driver:
            current["SDL_VIDEODRIVER"] = driver
        log_path = Path(directory) / "runtime.log"
        with log_path.open("w") as log:
            process = subprocess.Popen([binary, "--display", mode], cwd=directory,
                                       env=current, stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 15
                text = ""
                while time.monotonic() < deadline:
                    text = log_path.read_text()
                    match = re.search(r"web server listening on port (\d+)", text)
                    if match or process.poll() is not None:
                        break
                    time.sleep(0.02)
                if blocked or (mode == "desktop" and driver == "invalid-chiton-driver"):
                    assert process.wait(timeout=5) != 0, text
                    if blocked:
                        assert "unable to bind to final port 8080" in log_path.read_text()
                        assert "threads joined" in log_path.read_text()
                    else:
                        assert "display initialization failed" in text
                    return text
                assert process.poll() is None and match, text
                if expected_backend:
                    assert expected_backend in text, text
                port = int(match[1])
                assert request(port, "/").startswith(b"HTTP/1.1"), text
                # Exercise both file-backed frame sources and camera disable at shutdown.
                assert request(port, "/depth/setting/depth").startswith(b"HTTP/1.1 200")
                time.sleep(0.15)
                assert request(port, "/depth/setting/ir").startswith(b"HTTP/1.1 200")
                time.sleep(1.5)  # Let startup animation finish and present preview frames.
                assert request(port, "/camera/setting/disable").startswith(b"HTTP/1.1 200")
                # An idle HTTP client must not prevent coordinated shutdown.
                idle = socket.create_connection(("127.0.0.1", port), timeout=2)
                process.send_signal(signal.SIGTERM)
                assert process.wait(timeout=5) == 0, log_path.read_text()
                idle.close()
                assert "threads joined" in log_path.read_text()
                return text
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()

    # Verify port 80 success when privileges permit, otherwise permission fallback.
    probe = reserve(80)
    privileged = probe is not None
    if probe:
        probe.close()
    text = run(expected_backend="switching to headless")
    assert ("listening on port 80\n" if privileged else "falling back to port 8080") in text
    holder = reserve(80)
    try:
        assert "falling back to port 8080" in run(mode="headless")
        holder8080 = reserve(8080)
        try:
            run(blocked=True)
        finally:
            holder8080.close()
    finally:
        if holder:
            holder.close()
    run(driver="invalid-chiton-driver", expected_backend="switching to headless")
    run(mode="desktop", driver="invalid-chiton-driver")
    for _ in range(3):
        run(mode="auto", driver="dummy", expected_backend="display backend: desktop (SDL2)")
    print("desktop runtime tests passed" + (" (port 80 success skipped: unprivileged)" if not privileged else ""))
