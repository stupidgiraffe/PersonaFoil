#!/usr/bin/env python3
"""PersonaBridge v1 conformance fixture.

This is a test server, not the production bridge. It exists so PersonaFoil and future
PersonaBridge implementations can exercise the same HTTP/JSON/range contract without
provider credentials or network access.
"""

from __future__ import annotations

import argparse
import json
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CONTENT = (b"PERSONAFOIL-BRIDGE-RANGE-FIXTURE-" * 128)[:4096]
UPDATE_TITLE_ID = "0100ABCDEF120800"
BASE_TITLE_ID = "0100ABCDEF120000"

SOURCES = [
    {"id": "telegram-main", "name": "Telegram", "kind": "telegram", "health": "online", "priority": 10},
    {"id": "dbi-main", "name": "DBI HTTP", "kind": "dbi_http", "health": "online", "priority": 20},
]

RELEASES = [
    {
        "id": "rel-update-tg",
        "title_id": UPDATE_TITLE_ID,
        "content_type": "update",
        "version": 131072,
        "size": len(CONTENT),
        "display_name": "Fixture Update",
        "source_id": "telegram-main",
        "download_id": "demo-update",
        "languages": ["en", "ja"],
    },
    {
        "id": "rel-update-dbi",
        "title_id": UPDATE_TITLE_ID,
        "content_type": "update",
        "version": 131072,
        "size": len(CONTENT),
        "display_name": "Fixture Update",
        "source_id": "dbi-main",
        "download_id": "demo-update",
        "languages": ["en", "ja"],
    },
]


def json_bytes(value: object) -> bytes:
    return json.dumps(value, separators=(",", ":"), sort_keys=True).encode("utf-8")


def reconcile(payload: dict) -> dict:
    if payload.get("schema_version") != 1 or not isinstance(payload.get("installed"), list):
        raise ValueError("invalid reconcile request")

    installed_update = 0
    has_base = False
    for item in payload["installed"]:
        if not isinstance(item, dict):
            raise ValueError("invalid installed record")
        if item.get("title_id") == BASE_TITLE_ID and item.get("content_type") == "base":
            has_base = True
        if item.get("title_id") == UPDATE_TITLE_ID and item.get("content_type") == "update":
            version = item.get("version")
            if not isinstance(version, int) or version < 0:
                raise ValueError("invalid installed version")
            installed_update = max(installed_update, version)

    actions = []
    if has_base and installed_update < 131072:
        actions.append(
            {
                "title_id": UPDATE_TITLE_ID,
                "content_type": "update",
                "installed_version": installed_update,
                "latest_version": 131072,
                "reason": "update",
                "alternatives": [
                    {
                        "release_id": release["id"],
                        "source_id": release["source_id"],
                        "download_id": release["download_id"],
                        "version": release["version"],
                        "size": release["size"],
                    }
                    for release in RELEASES
                ],
            }
        )
    return {"schema_version": 1, "actions": actions, "warnings": []}


class Handler(BaseHTTPRequestHandler):
    server_version = "PersonaBridgeFixture/1"

    def log_message(self, _format: str, *_args: object) -> None:
        return

    def send_json(self, status: int, value: object) -> None:
        body = json_bytes(value)
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path == "/v1/health":
            self.send_json(200, {"schema_version": 1, "status": "ok", "sources": SOURCES})
            return
        if self.path == "/v1/catalog":
            self.send_json(
                200,
                {
                    "schema_version": 1,
                    "generated_at": "2026-10-04T00:00:00Z",
                    "sources": SOURCES,
                    "releases": RELEASES,
                },
            )
            return
        if self.path == "/v1/jobs":
            self.send_json(200, {"schema_version": 1, "jobs": []})
            return
        if self.path == "/v1/content/demo-update":
            self.send_content()
            return
        self.send_json(404, {"error": "not found"})

    def send_content(self) -> None:
        range_header = self.headers.get("Range")
        if not range_header:
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Length", str(len(CONTENT)))
            self.end_headers()
            self.wfile.write(CONTENT)
            return

        match = re.fullmatch(r"bytes=(\d+)-(\d+)", range_header.strip())
        if not match:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{len(CONTENT)}")
            self.end_headers()
            return

        first, last = map(int, match.groups())
        if first > last or first >= len(CONTENT):
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{len(CONTENT)}")
            self.end_headers()
            return
        last = min(last, len(CONTENT) - 1)
        body = CONTENT[first : last + 1]
        self.send_response(206)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Range", f"bytes {first}-{last}/{len(CONTENT)}")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:
        if self.path != "/v1/reconcile":
            self.send_json(404, {"error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > 1024 * 1024:
                raise ValueError("invalid content length")
            payload = json.loads(self.rfile.read(length))
            self.send_json(200, reconcile(payload))
        except (ValueError, json.JSONDecodeError):
            self.send_json(400, {"error": "invalid reconcile request"})


def serve(port: int) -> None:
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"PersonaBridge fixture listening on 127.0.0.1:{server.server_port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=0)
    args = parser.parse_args()
    serve(args.port)
