#!/usr/bin/env python3
import json
import threading
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

from personabridge_fixture import BASE_TITLE_ID, CONTENT, Handler


def fetch(url, *, method="GET", payload=None, headers=None):
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=body,
        method=method,
        headers={"Content-Type": "application/json", **(headers or {})},
    )
    with urllib.request.urlopen(request, timeout=5) as response:
        return response.status, dict(response.headers), response.read()


def main():
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = f"http://127.0.0.1:{server.server_port}"
    try:
        status, _, body = fetch(base + "/v1/catalog")
        catalog = json.loads(body)
        assert status == 200
        assert catalog["schema_version"] == 1
        assert len(catalog["sources"]) == 2
        assert len(catalog["releases"]) == 2

        status, _, body = fetch(
            base + "/v1/reconcile",
            method="POST",
            payload={
                "schema_version": 1,
                "installed": [
                    {"title_id": BASE_TITLE_ID, "content_type": "base", "version": 0}
                ],
            },
        )
        reconciled = json.loads(body)
        assert status == 200
        assert len(reconciled["actions"]) == 1
        assert len(reconciled["actions"][0]["alternatives"]) == 2

        status, headers, body = fetch(
            base + "/v1/content/demo-update",
            headers={"Range": "bytes=10-31"},
        )
        assert status == 206
        assert headers["Content-Range"] == f"bytes 10-31/{len(CONTENT)}"
        assert body == CONTENT[10:32]

        try:
            fetch(base + "/v1/content/demo-update", headers={"Range": "bytes=99999-100000"})
            raise AssertionError("expected 416")
        except urllib.error.HTTPError as error:
            assert error.code == 416

        status, _, body = fetch(base + "/v1/jobs")
        assert status == 200
        assert json.loads(body)["jobs"] == []
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

    print("PersonaBridge HTTP fixture tests passed")


if __name__ == "__main__":
    main()
