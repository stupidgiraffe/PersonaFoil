import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="personafoil-cache-") as temporary:
    subprocess.run([str(root / "build-host/catalog_tests"), str(root / "romfs/catalog-v1.json"), temporary], check=True)
    subprocess.run([str(root / "build-host/config_migration_tests"), temporary], check=True)
