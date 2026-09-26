#!/usr/bin/env bash
# Checks that the Flatpak manifest asks for the same dependency versions the
# build actually fetches.
#
# It drifted once already, and silently: the manifest pinned SDL3 at 3.2.0
# while CMakeLists.txt fetched 3.4.14, and pinned SDL_mixer to a branch,
# which Flathub does not accept. Nobody noticed because nobody had built it
# yet, and a manifest nobody builds is a document, not a recipe.
#
# Exits 77 (CTest's skip code) when PyYAML is not installed.
set -euo pipefail

root=$(cd -- "$(dirname -- "$0")/.." && pwd)

python3 -c "import yaml" 2>/dev/null || {
    echo "PyYAML is not installed, skipping"
    exit 77
}

python3 - "$root" <<'PY'
import re
import sys
from pathlib import Path

import yaml

root = Path(sys.argv[1])
cmake = (root / "CMakeLists.txt").read_text()
manifest = yaml.safe_load(
    (root / "packaging/flatpak/io.github.Esl1h.AbuseVrenna.yml").read_text())

def wanted(name):
    """The GIT_TAG CPMAddPackage uses for this package."""
    m = re.search(r"NAME\s+%s\b.*?GIT_TAG\s+(\S+)" % re.escape(name),
                  cmake, re.S)
    return m.group(1) if m else None

modules = {m["name"]: m for m in manifest["modules"]}
errors = []

for cpm_name, module, field in (("SDL3", "sdl3", "tag"),
                                ("SDL3_mixer", "sdl3-mixer", "tag")):
    want = wanted(cpm_name)
    if want is None:
        errors.append(f"{cpm_name}: no GIT_TAG in CMakeLists.txt")
        continue
    got = modules[module]["sources"][0].get(field)
    if got != want:
        errors.append(f"{cpm_name}: the build fetches {want}, "
                      f"the manifest pins {got}")
    if not modules[module]["sources"][0].get("commit"):
        errors.append(f"{cpm_name}: no commit pinned; Flathub needs one")
    if modules[module]["sources"][0].get("branch"):
        errors.append(f"{cpm_name}: pinned to a branch, which is not "
                      f"reproducible and Flathub refuses it")

# native_midi is not a module: it is source files compiled into the game,
# placed where CPM_SDL3_native_midi_SOURCE points.
want = wanted("SDL3_native_midi")
got = None
patched = False
for s in modules["abuse-vrenna"]["sources"]:
    if not s.get("dest", "").endswith("SDL_native_midi"):
        continue
    # By type, not by dest alone: the patch below sits at the same dest and
    # carries no commit, and reading that one turned this check red.
    if s.get("type") == "git":
        got = s.get("commit")
    elif s.get("type") == "patch":
        patched = True
if got != want:
    errors.append(f"SDL3_native_midi: the build fetches {want}, "
                  f"the manifest pins {got}")

# CPM applies this itself where it fetches the dependency. Here it does not
# fetch, so the manifest has to apply it, and without it the packaged game
# dies in the MIDI probe on a command line with no '/' in it.
if not patched:
    errors.append("SDL3_native_midi: the app name patch is not applied; "
                  "see third_party/patches/native-midi-app-name.patch")

# A Flathub build has no network, so anything CPM would fetch at configure
# time has to be switched off or supplied above.
opts = " ".join(modules["abuse-vrenna"]["config-opts"])
if "-DABUSE_BUILD_TESTS=OFF" not in opts:
    errors.append("doctest would be fetched at configure time: "
                  "-DABUSE_BUILD_TESTS=OFF is missing")
if "CPM_SDL3_native_midi_SOURCE" not in opts:
    errors.append("SDL3_native_midi would be fetched at configure time: "
                  "-DCPM_SDL3_native_midi_SOURCE is missing")

if errors:
    for e in errors:
        print(e, file=sys.stderr)
    sys.exit(1)

print("flatpak manifest ok: pins match the build")
PY
