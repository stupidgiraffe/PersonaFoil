"""Exercise the actual C++ curl client against bounded local HTTP/TLS servers."""
import http.server
import pathlib
import ssl
import subprocess
import sys
import tempfile
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_GET(self):
        status, body, headers = 200, b"", {}
        if self.path == "/range":
            value = self.headers.get("Range", "none")
            status, body = (206 if value != "none" else 200), value.encode()
        elif self.path == "/missing":
            status, body = 404, b"error body must not reach install writer"
        elif self.path == "/payload":
            body = b"test"
        elif self.path == "/slow":
            time.sleep(0.7)
        elif self.path == "/large":
            body = b"x" * 64
        elif self.path in ("/same", "/cross", "/cycle"):
            status = 302
            headers["Location"] = {"/same": "/credentials", "/cross": self.server.cross + "/credentials", "/cycle": "/cycle"}[self.path]
        elif self.path == "/credentials":
            present = any(self.headers.get(h) for h in ("Authorization", "HAUTH", "UAUTH", "UID"))
            body = b"present" if present else b"absent"
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Type", "text/plain")
        for name, value in headers.items():
            self.send_header(name, value)
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_HEAD(self):
        self.send_response(302 if self.path == "/cross" else 200)
        if self.path == "/cross":
            self.send_header("Location", self.server.cross + "/credentials")
        present = any(self.headers.get(h) for h in ("Authorization", "HAUTH", "UAUTH", "UID"))
        self.send_header("X-Credentials", "present" if present else "absent")
        self.send_header("Content-Length", "4")
        self.end_headers()


def start():
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    return server


with tempfile.TemporaryDirectory(prefix="personafoil-http-") as temporary:
    root = pathlib.Path(temporary)
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-subj", "/CN=localhost", "-keyout", str(root / "key"), "-out", str(root / "cert")], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    servers = [start() for _ in range(3)]
    servers[0].cross = f"http://127.0.0.1:{servers[1].server_port}"
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(root / "cert", root / "key")
    servers[2].socket = context.wrap_socket(servers[2].socket, server_side=True)
    for server in servers:
        threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        subprocess.run([sys.argv[1], f"http://127.0.0.1:{servers[0].server_port}", f"https://127.0.0.1:{servers[2].server_port}/empty"], check=True)
    finally:
        for server in servers:
            server.shutdown()
            server.server_close()
