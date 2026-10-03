"""Publish one validated catalog through an atomic Git ref update on catalog-data."""
import argparse
import base64
import json
import pathlib
import subprocess
from build_catalog import validate_previous

REPOSITORY = "stupidgiraffe/PersonaFoil"


def api(route, payload=None, method="GET"):
    command = ["gh", "api", f"repos/{REPOSITORY}/{route}", "--method", method]
    if payload is not None:
        command.extend(["--input", "-"])
    result = subprocess.run(command, input=json.dumps(payload) if payload is not None else None, text=True, capture_output=True, check=True)
    return json.loads(result.stdout)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog")
    args = parser.parse_args()
    text = pathlib.Path(args.catalog).read_text()
    validate_previous(json.loads(text))
    try:
        parent = api("git/ref/heads/catalog-data")["object"]["sha"]
    except subprocess.CalledProcessError:
        parent = None
    blob = api("git/blobs", {"content": base64.b64encode(text.encode()).decode(), "encoding": "base64"}, "POST")
    tree = api("git/trees", {"tree": [{"path": "catalog-v1.json", "mode": "100644", "type": "blob", "sha": blob["sha"]}]}, "POST")
    commit = api("git/commits", {"message": "chore(catalog): refresh public endpoint metadata", "tree": tree["sha"], "parents": [parent] if parent else []}, "POST")
    if parent:
        api("git/refs/heads/catalog-data", {"sha": commit["sha"], "force": False}, "PATCH")
    else:
        api("git/refs", {"ref": "refs/heads/catalog-data", "sha": commit["sha"]}, "POST")
    print(f'Published catalog commit {commit["sha"]}')
