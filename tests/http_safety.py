#!/usr/bin/env python3
"""Focused headless/stub HTTP regression test; run with a built bin/chiton."""
import concurrent.futures
import http.client
import io
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import time
from runtime_support import runtime_directory, simulator_environment

binary = Path(sys.argv[1] if len(sys.argv) > 1 else "bin/chiton").resolve()

with runtime_directory(prefix="chiton-http-safety-") as directory:
    env = simulator_environment(directory)
    env['ADITOF_STUB_FAIL_REGISTER'] = '1'
    for variable, value in (("ADITOF_STUB_IR", 1800), ("ADITOF_STUB_DEPTH", 800)):
        env[variable] = directory + "/" + variable + ".bin"
        Path(env[variable]).write_bytes(value.to_bytes(2, "little") * (640 * 480))
    env["CHITON_CALIBRATION_FILE"] = directory + "/calibration.bin"
    log_path = Path(directory) / "runtime.log"
    with log_path.open("w") as log:
        process = subprocess.Popen([str(binary), "--display", "headless"],
                                   cwd=directory, env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                match = re.search(r"web server listening on port (\d+)", log_path.read_text())
                if match or process.poll() is not None:
                    break
                time.sleep(.05)
            assert process.poll() is None and match, log_path.read_text()
            assert "file-backed camera: hardware temperature monitor disabled" in log_path.read_text(), \
                "This suite requires a simulator build; refusing hardware actions"
            port = int(match[1])

            def request(path, expected=200, method="GET", with_headers=False):
                with socket.create_connection(("127.0.0.1", port), timeout=12) as client:
                    client.sendall(f"{method} {path} HTTP/1.0\r\n\r\n".encode())
                    response = bytearray()
                    while chunk := client.recv(65536):
                        response.extend(chunk)
                statuses = (expected,) if isinstance(expected, int) else expected
                assert any(response.startswith(f"HTTP/1.1 {status} ".encode()) for status in statuses), (path, response[:200])
                assert response.count(b"HTTP/1.1 ") == 1, path
                headers, body = response.split(b"\r\n\r\n", 1)
                assert b"\n" not in headers.replace(b"\r\n", b""), path
                assert b"Connection: close" in headers, path
                length = re.search(rb"Content-Length: (\d+)", headers)
                assert length and len(body) == (0 if method == "HEAD" else int(length[1])), path
                class RecordedSocket:
                    def makefile(self, *args, **kwargs):
                        return io.BytesIO(response)
                parsed = http.client.HTTPResponse(RecordedSocket(), method=method)
                parsed.begin()
                assert parsed.status in statuses and parsed.read() == body, path
                if method != "HEAD" and b"application/json" in headers:
                    data = json.loads(body)
                    if parsed.status >= 400:
                        assert data["status"] == parsed.status and isinstance(data["error"], str), path
                if with_headers:
                    return body, {name.lower(): value for name, value in parsed.getheaders()}
                return body

            from http_framing import exercise as exercise_framing
            exercise_framing(port, request)

            def asset_contents(filename):
                if "CHITON_ASSET_DIR" in env:
                    return (Path(env["CHITON_ASSET_DIR"]) / filename).read_bytes()
                roots = [Path("/usr/local/chiton/current"), binary.parent,
                         binary.parent / "../release", binary.parent / "../../release"]
                for relative in ("files/" + filename, filename):
                    for root in roots:
                        path = root / relative
                        if path.is_file():
                            return path.read_bytes()
                raise AssertionError("missing test asset: " + filename)
            assert request("/") == asset_contents("app.html")
            assert request("/app.html?cache=123") == asset_contents("app.html")
            assert request("/app.html", method="HEAD") == b""
            request("/missing.css", 404)
            request("/missing", 404)
            for family in ("camera", "calibrate", "setting", "software", "rover"):
                request("/missing/" + family, 404)
            request("/quality/setting/75", 405, method="POST")
            request("/disable/camera", 405, method="HEAD")
            for asset in ("autosize.min.js", "feather.css"):
                assert request("/" + asset) == asset_contents(asset)
            # Complete arity validation occurs before mutation/hardware access.
            counts = dict(degree=3, quality=3, camera=3, depth=3, slam=3,
                          dynamic=3, calibration_angle=4, colormode=3, max=3, min=3,
                          smoothing=3, chitoncal=3, planar=3, noisereduction=3,
                          width=2, temp=2, radians=2, range=4, generalread=3, generalwrite=4,
                          calibration=3, wlan0=4, sdk_reload=2, gateway=3)
            for route, count in counts.items():
                parts = [route, "setting"] + ["1"] * (count - 2)
                if count > 2 and route not in ("depth", "colormode", "camera"):
                    request("/" + "/".join(parts[:-1]), 400)
                request("/" + "/".join(parts + ["extra"]), 400)
            for route in ("degree", "quality", "colormode", "max", "min", "noisereduction", "generalread"):
                for invalid in ("bogus", "9999999999999999999999999", "1garbage", "nan"):
                    request(f"/{route}/setting/{invalid}", 400)
            for path in ("/sample2/calibrate", "/sample2/calibrate/500/1/nope",
                         "/sample2/calibrate/500/999999999999999/1",
                         "/get/value/1/1/2", "/get/value/640/0", "/get/value/0/480",
                         "/get/value/-1/0", "/get/value/1x/0", "/get/value/0/0/0/0",
                         "/quality/setting/%", "/quality/setting/%gg",
                         "/upgrade/software", "/upgrade/software/1junk"):
                request(path, 400)
            # Every formerly malformed JSON success path is decoded by request().
            assert json.loads(request("/dynamic/setting/off"))["value"] is False
            assert json.loads(request("/slam/setting/on"))["value"] is True
            assert json.loads(request("/slam/setting/off"))["value"] is False
            assert json.loads(request("/degree/setting/%2B0100"))["value"] == 1
            request("/calibration_angle/setting/%2B0500/%2B0000")
            request("/degree/setting/0")
            request("/generalwrite/setting/c001/00ab")
            assert json.loads(request("/generalread/setting/c001"))["value"] == 0xab
            # Stub-only SDK failure at one reserved address, with recovery.
            request("/generalread/setting/c0ff", 500)
            request("/generalwrite/setting/c0ff/1234", 500)
            assert json.loads(request("/generalread/setting/c001"))["value"] == 0xab
            request("/gateway/setting/invalid", 400)
            request("/gateway/setting/wlan0", 501)
            request("/wlan0/setting/test/password", 501)
            request("/upgrade/software/42", 501)
            request("/debug/software", 501)
            request("/motorbegin/rover", 501)
            request("/disable/camera")
            request("/image/camera", 503)
            request("/enable/camera")
            # Disable calibration/smoothing to inspect distinct, unmodified fixtures.
            request("/chitoncal/setting/off")
            request("/smoothing/setting/off")
            for frame_type, value in (("depth", 800), ("ir", 1800), ("depth", 800)):
                selected = json.loads(request("/depth/setting/" + frame_type))
                assert selected == {"value": frame_type == "depth", "frame_type": frame_type}
                assert json.loads(request("/depth/setting")) == selected
                deadline = time.monotonic() + 3
                while True:
                    raw = request("/raw/camera")
                    if int.from_bytes(raw[:2], "little") == value:
                        break
                    assert time.monotonic() < deadline, (frame_type, raw[:2])
            request("/depth/setting/ir")
            request("/calibration/setting/off")
            assert json.loads(request("/depth/setting"))["frame_type"] == "ir"
            request("/depth/setting/depth")
            from http_exports import exercise as exercise_exports
            exercise_exports(request)
            from http_ui_contracts import exercise as exercise_ui_contracts
            exercise_ui_contracts(request)
            request("/min/setting/100")
            request("/max/setting/200")
            request("/min/setting/200", 400)
            request("/max/setting/100", 400)
            assert json.loads(request("/range/setting")) == {"min": 100, "max": 200}
            request("/range/setting/300/200", 400)
            request("/range/setting/100/bogus", 400)
            assert json.loads(request("/range/setting")) == {"min": 100, "max": 200}
            assert json.loads(request("/range/setting/7000/8000")) == {"min": 7000, "max": 8000}
            request("/range/setting/0/6500")
            from http_modes import exercise as exercise_modes
            exercise_modes(request)
            from http_freshness import exercise as exercise_freshness
            exercise_freshness(request, directory)
            from http_image_stream import exercise as exercise_image_stream
            exercise_image_stream(port, request)
            request("/max/setting/6500")
            request("/min/setting/0")
            # Calibration file errors return JSON, not a crash or partial publication.
            request("/load/calibrate", 500)
            request("/save/calibrate")
            saved = Path(env["CHITON_CALIBRATION_FILE"]).read_bytes()
            assert saved.startswith(b"CHITCAL1")
            Path(env["CHITON_CALIBRATION_FILE"]).write_bytes(saved[:-1])
            request("/load/calibrate", 500)
            Path(env["CHITON_CALIBRATION_FILE"]).write_bytes(saved)
            request("/load/calibrate")
            # A directory at the target forces both file-load and atomic-rename
            # failures, even as root. Preserve active coefficients and recover.
            path = Path(env["CHITON_CALIBRATION_FILE"])
            path.unlink()
            path.mkdir()
            request("/load/calibrate", 500)
            request("/save/calibrate", 500)
            assert path.is_dir() and not list(path.parent.glob("calibration.bin.tmp*"))
            path.rmdir()
            path.write_bytes(saved)
            request("/load/calibrate")
            request("/save/calibrate")
            assert path.read_bytes() == saved
            request("/smooth/calibrate")
            request("/process/calibrate")
            request("/sample2/calibrate/500/1/0")
            request("/degree/setting/0")
            request("/get/value/639/479")
            request("/get/value/640/480/0/0")  # Reversed, exclusive endpoints.
            request("/colormode/setting/5")
            request("/get/value/1500/0", 400)
            request("/get/value/0/165", 400)
            setting = json.loads(request("/colormode/setting/6"))
            assert request("/image/camera?after=" + str(setting["frame_id"])).startswith(b"\xff\xd8")
            setting = json.loads(request("/colormode/setting/1"))
            assert len(request("/raw/camera")) == 640 * 480 * 2
            assert request("/image/camera?after=" + str(setting["frame_id"])).startswith(b"\xff\xd8")
            request("/pointcloud/camera", 409)
            previous = json.loads(request("/frame/camera"))["configuration_revision"]
            assert json.loads(request("/colormode/setting/4"))["value"] == 4
            deadline = time.monotonic() + 8
            while True:
                metadata = json.loads(request("/frame/camera"))
                if metadata["configuration_revision"] > previous and metadata["color_mode"] == 4:
                    break
                assert time.monotonic() < deadline, metadata
                time.sleep(.02)
            request("/pointcloud/camera")
            request("/colormode/setting/1")
            request("/sdk_reload/setting", 409)

            def exercise(index):
                for _ in range(5):
                    request(f"/colormode/setting/{1 + index % 3}")
                    request("/quality/setting/75")
                    request("/get/value/10/10")
                    assert len(request("/raw/camera")) == 640 * 480 * 2
                    assert request("/image/camera?after=0").startswith(b"\xff\xd8")
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
                list(executor.map(exercise, range(4)))
            assert process.poll() is None
            assert json.loads(request("/restart/software"))["restart"] == 1
            # EXIT_UPGRADE retains the legacy nonzero exit used by the device launcher.
            assert process.wait(timeout=5) == 1, log_path.read_text()
            assert "threads joined" in log_path.read_text()
            # Startup uses exactly the file saved by HTTP. Bad input is nonfatal.
            for payload, invalid in ((saved, False), (b"truncated", True)):
                Path(env["CHITON_CALIBRATION_FILE"]).write_bytes(payload)
                log_path = Path(directory) / ("restart-invalid.log" if invalid else "restart-valid.log")
                with log_path.open("w") as restart_log:
                    restarted = subprocess.Popen([str(binary), "--display", "headless"],
                                                 cwd=directory, env=env, stdout=restart_log, stderr=restart_log)
                    try:
                        deadline = time.monotonic() + 20
                        while time.monotonic() < deadline:
                            text = log_path.read_text()
                            if "web server listening" in text or restarted.poll() is not None:
                                break
                            time.sleep(.05)
                        assert restarted.poll() is None and "web server listening" in text, text
                        assert ("ignoring invalid calibration" in text) == invalid, text
                        restart_port = int(re.search(r"web server listening on port (\d+)", text)[1])
                        with socket.create_connection(("127.0.0.1", restart_port), timeout=3) as incomplete:
                            if invalid:
                                incomplete.sendall(b"GET /frame/camera?after=18446744073709551615 HTTP/1.0\r\n\r\n")
                            else:
                                incomplete.sendall(b"GET /range/setting/123/456 HTTP/1.0\r\nX: pending")
                            time.sleep(.1)
                            started = time.monotonic()
                            restarted.send_signal(signal.SIGTERM)
                            assert restarted.wait(timeout=5) == 0, log_path.read_text()
                            assert time.monotonic() - started < 2
                            reply = incomplete.recv(1024)
                            assert reply == b"" if not invalid else (reply == b"" or reply.startswith(b"HTTP/1.1 503 "))
                    finally:
                        if restarted.poll() is None:
                            restarted.kill(); restarted.wait()
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
print("HTTP safety tests passed")
