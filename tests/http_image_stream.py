"""RFC6455/credit/backpressure integration against the local simulator only."""
import base64
import json
import os
import socket
import struct
import time


def exercise(port, request):
    def connect(origin=None, key=None, expected=101):
        sock = socket.create_connection(('127.0.0.1', port), timeout=5)
        key = key or base64.b64encode(os.urandom(16)).decode()
        host = f'127.0.0.1:{port}'
        headers = (f'GET /stream/camera HTTP/1.1\r\nHost: {host}\r\nConnection: Upgrade\r\n'
                   f'Upgrade: websocket\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n'
                   f'Sec-WebSocket-Protocol: chiton.jpeg.v1\r\n')
        if origin:
            headers += f'Origin: {origin}\r\n'
        sock.sendall((headers+'\r\n').encode())
        reader = sock.makefile('rb', buffering=0)
        status = reader.readline()
        assert int(status.split()[1]) == expected, status
        while reader.readline() != b'\r\n':
            pass
        return sock, reader

    def receive(reader):
        def exact(n):
            data = b''
            while len(data) < n:
                chunk = reader.read(n-len(data))
                assert chunk, 'Unexpected WebSocket EOF'
                data += chunk
            return data
        first, length = exact(2)
        assert first & 0x80 and not length & 0x80
        size = length & 127
        if size == 126:
            size = struct.unpack('!H', exact(2))[0]
        elif size == 127:
            size = struct.unpack('!Q', exact(8))[0]
        assert size <= 2*1024*1024+4100
        return first & 15, exact(size)

    def send(sock, payload, opcode=1):
        mask = b'abcd'
        assert len(payload) <= 125
        sock.sendall(bytes([0x80 | opcode, 0x80 | len(payload)]) + mask +
                     bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    request('/colormode/setting/1')
    peers = []
    try:
        bad, file = connect(key='bad', expected=400); file.close(); bad.close()
        bad, file = connect(origin='http://evil.invalid', expected=403); file.close(); bad.close()
        for _ in range(4):
            peers.append(connect())
        bad, file = connect(expected=503); file.close(); bad.close()
        sock, file = peers[0]
        previous = 0
        for _ in range(8):
            while True:
                opcode, payload = receive(file)
                if opcode == 2:
                    break
                assert opcode == 1 and json.loads(payload)['mode'] == 1
            size = struct.unpack('!I', payload[:4])[0]
            meta = json.loads(payload[4:4+size])
            jpeg = payload[4+size:]
            assert meta['version'] == 1 and meta['mode'] == 1
            assert meta['width'] == 640 and meta['height'] == 480
            assert meta['frame_id'] > previous and jpeg.startswith(b'\xff\xd8') and jpeg.endswith(b'\xff\xd9')
            previous = meta['frame_id']
            send(sock, str(previous).encode())
        # Ping/pong while frame credit is outstanding; pong is not a JPEG ACK.
        send(sock, b'ping', 9)
        while True:
            opcode, payload = receive(file)
            if opcode == 10:
                assert payload == b'ping'; break
            if opcode == 2:
                size = struct.unpack('!I', payload[:4])[0]
                send(sock, str(json.loads(payload[4:4+size])['frame_id']).encode())
        stats = json.loads(request('/stream/metrics'))
        assert stats['client_limit'] == 4 and stats['acknowledged_frames'] >= 8
        assert stats['pipeline']['jpeg_frames'] >= 8 and stats['pipeline']['encode_ms'] >= 0
        # Capture/global-mode remain explicit; cloud mode never pushes JPEGs.
        request('/colormode/setting/4')
        until = time.monotonic()+3
        while True:
            opcode, payload = receive(file)
            if opcode == 1 and json.loads(payload)['mode'] == 4:
                break
            if opcode == 2:
                size = struct.unpack('!I', payload[:4])[0]
                send(sock, str(json.loads(payload[4:4+size])['frame_id']).encode())
            assert time.monotonic() < until
        assert json.loads(request('/colormode/setting'))['value'] == 4
        until = time.monotonic()+3
        while json.loads(request('/stream/metrics'))['slow_disconnects'] < 1:
            assert time.monotonic() < until, 'Unacknowledged viewers were not disconnected'
            time.sleep(0.05)  # Other three never ACKed; their two-second credit expires.
    finally:
        for sock, file in peers:
            file.close(); sock.close()
        request('/colormode/setting/1')
