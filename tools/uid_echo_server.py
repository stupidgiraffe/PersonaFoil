#!/usr/bin/env python3
"""Minimal controlled endpoint for PersonaFoil UID verification."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class UidEchoHandler(BaseHTTPRequestHandler):
    server_version = "PersonaFoilUidEcho/0.1"

    def _handle(self, include_body: bool) -> None:
        event = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "client": self.headers.get("User-Agent", ""),
            "uid": self.headers.get("UID", self.headers.get("Uid", "")),
            "path": self.path,
        }
        print(json.dumps(event, ensure_ascii=False), flush=True)

        body = (json.dumps(event, indent=2) + "\n").encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if include_body:
            self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        self._handle(include_body=True)

    def do_HEAD(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        self._handle(include_body=False)

    def log_message(self, _format: str, *_args: object) -> None:
        # The structured event above intentionally excludes Authorization and
        # every other request header.
        return


def main() -> None:
    parser = argparse.ArgumentParser(description="Echo safe PersonaFoil UID request metadata.")
    parser.add_argument("--bind", default="0.0.0.0", help="Address to bind (default: all LAN interfaces)")
    parser.add_argument("--port", default=8080, type=int, help="TCP port (default: 8080)")
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.bind, args.port), UidEchoHandler)
    print(f"PersonaFoil UID echo server listening on http://{args.bind}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
