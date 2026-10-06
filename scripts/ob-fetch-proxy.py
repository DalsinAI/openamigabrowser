#!/usr/bin/env python3
"""ob-fetch-proxy: the host-fetch experiment for OpenBrowser on AmigaChrome.

OpenBrowser, with the Shell variable OB_FETCH_PROXY set to this proxy
("Set OB_FETCH_PROXY http://127.0.0.1:8810"), sends its http and https
requests here as plain HTTP; https ones carry "X-OB-Fetch-Scheme: https".
The proxy makes the real connection, TLS included, with the PC's CPU and
trust store, and returns the response as it came. Cookies, redirects and
caching stay with WebCore on the Amiga.

    python3 scripts/ob-fetch-proxy.py [--listen ADDRESS:PORT] [--allow IP ...]

For testing only. AmigaChrome lets an instance reach the internet and the
LAN but not the PC it runs on, so the proxy runs on another PC on the LAN:
"--listen 192.168.0.168:8810 --allow 192.168.0.195" serves the instances of
the PC at 192.168.0.195 and refuses everyone else.
MIT, Copyright (c) 2026 Dalsin Limited.
"""
import argparse
import http.client
import http.server
import socket
import ssl
import threading
import time
import urllib.parse

HOP_BY_HOP = {"connection", "proxy-connection", "keep-alive", "proxy-authorization", "proxy-authenticate",
              "te", "trailer", "transfer-encoding", "upgrade", "x-ob-fetch-scheme"}
TLS = ssl.create_default_context()


class Pool:
    """Upstream connections kept open per (scheme, host, port)."""

    def __init__(self):
        self.lock = threading.Lock()
        self.idle = {}

    def take(self, key):
        with self.lock:
            free = self.idle.get(key)
            if free:
                return free.pop(), True
        scheme, host, port = key
        if scheme == "https":
            return http.client.HTTPSConnection(host, port, timeout=60, context=TLS), False
        return http.client.HTTPConnection(host, port, timeout=60), False

    def give(self, key, connection):
        with self.lock:
            self.idle.setdefault(key, []).append(connection)


POOL = Pool()
ALLOWED = {"127.0.0.1"}


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def forward(self):
        if self.client_address[0] not in ALLOWED:
            self.fail(403, "proxy", f"{self.client_address[0]} is not allowed")
            return
        start = time.monotonic()
        parts = urllib.parse.urlsplit(self.path)
        scheme = "https" if self.headers.get("X-OB-Fetch-Scheme", "").lower() == "https" else "http"
        if not parts.hostname:
            self.fail(400, "proxy", "absolute URL expected")
            return
        port = parts.port or (443 if scheme == "https" else 80)
        target = urllib.parse.urlunsplit(("", "", parts.path or "/", parts.query, ""))
        body = self.read_body()
        headers = {"Host": parts.hostname if port == (443 if scheme == "https" else 80) else f"{parts.hostname}:{port}"}
        for name, value in self.headers.items():
            if name.lower() not in HOP_BY_HOP and name.lower() != "host":
                headers[name] = value if name not in headers else headers[name] + ", " + value
        key = (scheme, parts.hostname, port)
        for attempt in (1, 2):
            connection, reused = POOL.take(key)
            try:
                connection.request(self.command, target, body=body, headers=headers)
                response = connection.getresponse()
                data = response.read()
                break
            except (http.client.RemoteDisconnected, BrokenPipeError, ConnectionResetError) as error:
                connection.close()
                if reused and attempt == 1:
                    continue
                self.fail(502, "connect", str(error))
                return
            except ssl.SSLError as error:
                connection.close()
                self.fail(502, "tls", str(error))
                return
            except socket.gaierror as error:
                connection.close()
                self.fail(502, "dns", str(error))
                return
            except OSError as error:
                connection.close()
                self.fail(504 if isinstance(error, TimeoutError) else 502, "connect", str(error))
                return
        if response.will_close:
            connection.close()
        else:
            POOL.give(key, connection)
        self.send_response(response.status, response.reason)
        for name, value in response.getheaders():
            if name.lower() not in HOP_BY_HOP and name.lower() != "content-length":
                self.send_header(name, value)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)
        print(f"{self.command} {scheme}://{parts.hostname}{target} {response.status} {len(data)} bytes "
              f"{(time.monotonic() - start) * 1000:.0f} ms{' (reused)' if reused else ''}", flush=True)

    def read_body(self):
        length = self.headers.get("Content-Length")
        if length:
            return self.rfile.read(int(length))
        if self.headers.get("Transfer-Encoding", "").lower() == "chunked":
            chunks = []
            while True:
                size = int(self.rfile.readline().split(b";")[0], 16)
                if not size:
                    self.rfile.readline()
                    return b"".join(chunks)
                chunks.append(self.rfile.read(size))
                self.rfile.readline()
        return None

    def fail(self, status, kind, message):
        text = f"ob-fetch-proxy: {kind} error: {message}\n".encode()
        self.send_response(status)
        self.send_header("X-OB-Fetch-Error", f"{kind}: {message}"[:200])
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(text)))
        self.end_headers()
        self.wfile.write(text)
        print(f"{self.command} {self.path} failed: {kind}: {message}", flush=True)

    do_GET = do_POST = do_HEAD = do_PUT = do_DELETE = do_OPTIONS = do_PATCH = forward


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--listen", default="127.0.0.1:8810", help="address:port to listen on")
    parser.add_argument("--allow", nargs="*", default=[], help="client addresses to serve besides 127.0.0.1")
    args = parser.parse_args()
    ALLOWED.update(args.allow)
    host, port = args.listen.rsplit(":", 1)
    server = http.server.ThreadingHTTPServer((host, int(port)), Handler)
    server.daemon_threads = True
    print(f"ob-fetch-proxy on {host}:{port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
