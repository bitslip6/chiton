#!/usr/bin/env python3
"""Static, loopback-only console preview. Never starts or forwards camera APIs."""
import argparse
import functools
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

ASSETS = Path(__file__).resolve().parents[1] / 'release' / 'files'

class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        url = urlsplit(self.path)
        if url.scheme or url.netloc or self.client_address[0] not in ('127.0.0.1', '::1'):
            self.send_error(403)
            return
        if self.path == '/':
            self.send_response(302)
            self.send_header('Location', '/console.html?source=demo')
            self.end_headers()
            return
        # Flat deployable files only. No API mock/forwarder in the preview server.
        if '/' in url.path.lstrip('/') or not (ASSETS / url.path.lstrip('/')).is_file():
            self.send_error(404, 'Static preview only; no camera APIs')
            return
        super().do_GET()

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        super().end_headers()

    def do_CONNECT(self):
        self.send_error(403)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', choices=['127.0.0.1'], default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8765)
    args = parser.parse_args()
    with ThreadingHTTPServer((args.host, args.port), functools.partial(Handler, directory=str(ASSETS))) as server:
        print(f'Console preview: http://{args.host}:{server.server_port}/console.html?source=demo&scenario=stable&seed=7', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass

if __name__ == '__main__':
    main()
