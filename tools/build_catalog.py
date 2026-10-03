"""Public endpoint metadata only. No file lists, credentials or identity headers are published."""
import argparse
import datetime
import hashlib
import ipaddress
import json
import os
import pathlib
import socket
import urllib.error
import urllib.parse
import urllib.request

MAX_BYTES = 256 * 1024
MAX_ENTRIES = 256


def canonical(url):
    parsed = urllib.parse.urlsplit(url.strip())
    if parsed.scheme not in ("http", "https") or not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise ValueError("Endpoint is not a credential-free HTTP(S) address")
    if any(ord(char) < 33 or ord(char) == 127 for char in url):
        raise ValueError("Invalid endpoint characters")
    port = parsed.port or (443 if parsed.scheme == "https" else 80)
    host = parsed.hostname.lower()
    if ":" in host:
        host = f"[{host}]"
    path = parsed.path or "/"
    # Resolving dot segments avoids canonical duplicates and recursive aliases.
    path = urllib.parse.urlsplit(urllib.parse.urljoin(f"{parsed.scheme}://{host}:{port}/", path)).path
    default = 443 if parsed.scheme == "https" else 80
    authority = host if port == default else f"{host}:{port}"
    return f"{parsed.scheme}://{authority}{path}"


def public_endpoint(url):
    parsed = urllib.parse.urlsplit(canonical(url))
    for _family, _type, _proto, _name, address in socket.getaddrinfo(parsed.hostname, parsed.port or (443 if parsed.scheme == "https" else 80), type=socket.SOCK_STREAM):
        if not ipaddress.ip_address(address[0]).is_global:
            raise ValueError("Discovery probes require public addresses")


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *_args, **_kwargs):
        return None


def probe(url):
    original = canonical(url)
    current = original
    opener = urllib.request.build_opener(NoRedirect())
    for _ in range(6):
        try:
            public_endpoint(current)
            request = urllib.request.Request(current, headers={"User-Agent": "PersonaFoil-catalog/1", "Accept": "application/json"})
            try:
                response = opener.open(request, timeout=5)
            except urllib.error.HTTPError as error:
                response = error
            with response:
                status = response.code
                if status in (301, 302, 303, 307, 308):
                    target = canonical(urllib.parse.urljoin(current, response.headers.get("Location", "")))
                    if current.startswith("https:") and not target.startswith("https:"):
                        return {"health": "invalid_response", "http_result": status, "redirect_target": ""}, None
                    current = target
                    continue
                metadata = {"http_result": status, "redirect_target": current if current != original else ""}
                if status in (401, 403):
                    metadata["health"] = "authentication_required" if status == 401 else "degraded"
                    return metadata, None
                if status >= 400:
                    metadata["health"] = "degraded" if status == 429 else "offline"
                    return metadata, None
                body = response.read(MAX_BYTES + 1)
                if len(body) > MAX_BYTES:
                    metadata["health"] = "invalid_response"
                    return metadata, None
                try:
                    node = json.loads(body)
                except (ValueError, UnicodeError):
                    node = None
                valid = isinstance(node, dict) and any(key in node for key in ("files", "directories", "sections", "paths", "titledb"))
                metadata["health"] = "online" if valid else "invalid_response"
                return metadata, node if valid else None
        except (OSError, ValueError, urllib.error.URLError):
            return {"health": "offline", "http_result": 0, "redirect_target": ""}, None
    return {"health": "invalid_response", "http_result": 0, "redirect_target": ""}, None


def entry(url, title, source, provenance, now, previous=None):
    url = canonical(url)
    parsed = urllib.parse.urlsplit(url)
    host = parsed.hostname
    if ":" in host:
        host = f"[{host}]"
    return {
        "id": hashlib.sha256(url.encode()).hexdigest()[:24], "title": title[:80],
        "protocol": parsed.scheme, "host": host, "port": parsed.port or (443 if parsed.scheme == "https" else 80),
        "path": parsed.path or "/", "compatibility": "auto", "authentication": "unknown",
        "source_url": canonical(source), "provenance": provenance[:80],
        "discovered_at": previous["discovered_at"] if previous else now,
        "last_checked_at": previous["last_checked_at"] if previous else "",
        "health": previous["health"] if previous else "unknown", "http_result": previous["http_result"] if previous else 0,
        "redirect_target": previous.get("redirect_target", "") if previous else "", "tags": ["catalog"]
    }


def envelope(entries, now):
    serialized = json.dumps(entries, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    result = {"schema_version": 1, "generated_at": now, "entries": entries, "sha256": hashlib.sha256(serialized.encode()).hexdigest()}
    if len(json.dumps(result, ensure_ascii=False).encode()) > MAX_BYTES:
        raise ValueError("Catalog exceeds client limit")
    return result


def validate_previous(previous):
    if previous.get("schema_version") != 1 or not isinstance(previous.get("entries"), list) or len(previous["entries"]) > MAX_ENTRIES:
        raise ValueError("Invalid previous catalog")
    expected = envelope(previous["entries"], previous["generated_at"])["sha256"]
    if previous.get("sha256") != expected:
        raise ValueError("Previous catalog checksum mismatch")
    for value in previous["entries"]:
        allowed = set(entry("https://example.com/", "Example", "https://example.com/", "Fixture", "", None))
        if set(value) != allowed:
            raise ValueError("Previous catalog has unknown or sensitive fields")
        canonical(f'{value["protocol"]}://{value["host"]}:{value["port"]}{value["path"]}')
        canonical(value["source_url"])
    return previous


def build(sources, previous=None, fetch=probe, now=None, offline=False):
    now = now or datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    previous = validate_previous(previous) if previous else {"entries": []}
    known = {canonical(f'{value["protocol"]}://{value["host"]}:{value["port"]}{value["path"]}'): value for value in previous["entries"]}
    discovered = dict(known) # A failed feed never drops a last-good endpoint.
    for source in sources:
        url = canonical(source["url"])
        discovered[url] = entry(url, source["title"], url, source["name"], now, known.get(url))
        if offline:
            continue
        metadata, node = fetch(url)
        discovered[url].update(metadata, last_checked_at=now)
        if source.get("discover_directories") and isinstance(node, dict):
            directories = node.get("directories", [])
            if not isinstance(directories, list):
                directories = []
            for child in directories[:MAX_ENTRIES]:
                child_url = child if isinstance(child, str) else child.get("url", child.get("path", "")) if isinstance(child, dict) else ""
                try:
                    child_url = canonical(urllib.parse.urljoin(url, child_url))
                except ValueError:
                    continue
                if len(discovered) >= MAX_ENTRIES:
                    break
                if child_url not in discovered:
                    discovered[child_url] = entry(child_url, urllib.parse.urlsplit(child_url).hostname, url, source["name"], now, known.get(child_url))
    if not offline:
        # Metadata probes have a fixed request budget; the console never scrapes.
        source_urls = {canonical(source["url"]) for source in sources}
        for url in sorted(discovered)[:64]:
            if url in source_urls:
                continue
            metadata, _node = fetch(url)
            if metadata["health"] == "offline" and discovered[url]["health"] == "online":
                metadata["health"] = "degraded"
            discovered[url].update(metadata, last_checked_at=now)
    for value in discovered.values():
        value["authentication"] = "required" if value["health"] == "authentication_required" else "none" if value["health"] == "online" else "unknown"
    return envelope([discovered[url] for url in sorted(discovered)], now)


def write_atomic(path, catalog):
    temporary = path.with_suffix(path.suffix + ".new")
    with temporary.open("w", encoding="utf-8") as output:
        json.dump(catalog, output, sort_keys=True, indent=2, ensure_ascii=False)
        output.write("\n")
        output.flush()
        os.fsync(output.fileno())
    validate_previous(json.loads(temporary.read_text()))
    os.replace(temporary, path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--sources", default="tools/catalog_sources.json")
    parser.add_argument("--previous")
    parser.add_argument("--output", required=True)
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    previous = json.loads(pathlib.Path(args.previous).read_text()) if args.previous and pathlib.Path(args.previous).exists() else None
    result = build(json.loads(pathlib.Path(args.sources).read_text())["sources"], previous, offline=args.offline)
    write_atomic(pathlib.Path(args.output), result)
    print(f'Catalog v1: {len(result["entries"])} public endpoints; checksum validated')
