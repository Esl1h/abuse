#!/usr/bin/env bash
# Checks data/MANIFEST.toml against the files in data/. Fails when:
#   - a file under data/ has no entry in the manifest
#   - an entry uses a licence outside the allowed list
#   - any file exists under classic/ (original data never ships)
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
manifest="$root/data/MANIFEST.toml"

[ -f "$manifest" ] || { echo "missing $manifest" >&2; exit 1; }

python3 - "$manifest" "$root/data" <<'EOF'
import fnmatch
import sys
import tomllib
from pathlib import Path

manifest_path = Path(sys.argv[1])
data_dir = Path(sys.argv[2])

with open(manifest_path, "rb") as f:
    manifest = tomllib.load(f)

allowed = set(manifest["meta"]["allowed_licenses"])
ignore = set(manifest["meta"]["ignore"])
assets = manifest["asset"]

exact = {a["path"]: a for a in assets if "*" not in a["path"]}
globs = [a for a in assets if "*" in a["path"]]

errors = []

for asset in assets:
    if asset["license"] not in allowed:
        errors.append(f"licence not allowed: {asset['path']}: {asset['license']}")

for classic in data_dir.glob("classic/**"):
    if classic.is_file():
        errors.append(f"original data file in the package: {classic}")

for path in sorted(data_dir.rglob("*")):
    if not path.is_file():
        continue
    rel = path.relative_to(data_dir).as_posix()
    if rel in ignore:
        continue
    asset = exact.get(rel)
    if asset is None:
        asset = next(
            (a for a in globs if fnmatch.fnmatch(rel, a["path"])), None
        )
    if asset is None:
        errors.append(f"no manifest entry: data/{rel}")

if errors:
    for e in errors:
        print(e, file=sys.stderr)
    sys.exit(1)

print(f"manifest ok: {len(assets)} entries")
EOF
