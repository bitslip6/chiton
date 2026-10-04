"""Raw-socket F07 regressions, called only after simulator verification."""
import json
import socket
import time


def exercise(port, request):
    target = b"GET /range/setting/123/456 HTTP/1.1\r\nHost: localhost\r\n"

    def exchange(chunks, delay=0):
        with socket.create_connection(("127.0.0.1", port), timeout=3) as client:
            client.settimeout(3)
            for chunk in chunks:
                try:
                    client.sendall(chunk)
                except (BrokenPipeError, ConnectionResetError):
                    break
                if delay:
                    time.sleep(delay)
            data = b""
            while True:
                try:
                    chunk = client.recv(4096)
                except ConnectionResetError:
                    break
                if not chunk:
                    return data
                data += chunk
            return data

    def unchanged():
        assert json.loads(request("/range/setting")) == {"min": 10, "max": 900}

    request("/range/setting/10/900")
    # Every byte boundary, including request line and CRLF terminator, is fragmented.
    valid = target + b"Content-Length: 0\r\n\r\n"
    assert exchange([bytes([c]) for c in valid], .001).startswith(b"HTTP/1.1 200 ")
    assert json.loads(request("/range/setting")) == {"min": 123, "max": 456}
    request("/range/setting/10/900")
    cases = [
        (target + b"Content-Length: 1\r\n\r\nx", 400),
        (target + b"Content-Length: 1\r\n\r\n", 413),
        (target + b"Content-Length: 9999999999999999999999999\r\n\r\n", 413),
        (target + b"Content-Length: 0\r\nContent-Length: 0\r\n\r\n", 400),
        (target + b"Content-Length: 0, 0\r\n\r\n", 400),
        (target + b"Content-Length: +0\r\n\r\n", 400),
        (target + b"Content-Length: -1\r\n\r\n", 400),
        (target + b"Content-Length: 1x\r\n\r\n", 400),
        (target + b"Content-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n", 400),
        (target + b"Transfer-Encoding: identity\r\n\r\n", 400),
        (target + b"Expect: 100-continue\r\n\r\n", 400),
        (target + b" Host: folded\r\n\r\n", 400),
        (target + b"Name : value\r\n\r\n", 400),
        (target + b"Name: a\x00b\r\n\r\n", 400),
        (target + b"Name: a\x7fb\r\n\r\n", 400),
        (target + b"Host: duplicate\r\n\r\n", 400),
        (target.replace(b"Host: localhost\r\n", b"") + b"\r\n", 400),
        (target.replace(b"GET ", b"GET\t") + b"\r\n", 400),
        (target.replace(b"\r\n", b"\n") + b"\n", 400),
        (target + b"\r\nGET / HTTP/1.0\r\n\r\n", 400),
        (target + b"X: " + b"a" * 4096 + b"\r\n\r\n", 400),
        (target + b"X: a\r\n" * 100 + b"\r\n", 400),
        (target + b"X: " + b"a" * 17000, 431),
        (b"GET /" + b"a" * 4096 + b" HTTP/1.0\r\n\r\n", 400),
    ]
    for payload, status in cases:
        response = exchange([payload])
        assert response.startswith(f"HTTP/1.1 {status} ".encode()), (payload[:100], response)
        unchanged()
    # Declared bodies are rejected even when sent separately/fragmented; never dispatch.
    response = exchange([target, b"Content-Length: 3\r\n", b"\r\n", b"a", b"b", b"c"], .02)
    assert response.startswith(b"HTTP/1.1 413 "), response
    unchanged()
    for partial in (target[:12], target, target + b"Content-Length: 3\r\n"):
        with socket.create_connection(("127.0.0.1", port), timeout=3) as client:
            client.sendall(partial)
            client.shutdown(socket.SHUT_WR)
            assert client.recv(1024) == b""
        unchanged()
    start = time.monotonic()
    with socket.create_connection(("127.0.0.1", port), timeout=3) as trickle:
        trickle.settimeout(.12)
        trickle.sendall(target)
        response = b""
        while not response and time.monotonic() - start < 3:
            trickle.sendall(b"X")
            try:
                response = trickle.recv(4096)
            except socket.timeout:
                pass
        elapsed = time.monotonic() - start
    assert response.startswith(b"HTTP/1.1 408 ") and .8 <= elapsed < 1.5, (elapsed, response)
    unchanged()
    assert exchange([target]).startswith(b"HTTP/1.1 408 ")
    unchanged()
    # Header limit is inclusive; exactly 16 KiB valid input dispatches.
    base = target + b"X: " + b"a" * 4080 + b"\r\n"
    base += (b"Y: " + b"b" * 4080 + b"\r\n") * 2
    remaining = 16384 - len(base) - len(b"Z: \r\n\r\n")
    assert exchange([base + b"Z: " + b"c" * remaining + b"\r\n\r\n"]).startswith(b"HTTP/1.1 200 ")
    request("/range/setting/0/6500")
    print("HTTP framing tests passed")
