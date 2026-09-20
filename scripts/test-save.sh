#!/usr/bin/env bash
# Writes a savegame and checks that a file of a sensible size came out.
#
# Saving crashed on Windows the first time a person tried it, and nothing in
# the suite had ever written a save file: the path it writes to is decided by
# the platform, so the only way to test it is to run it on the platform.
#
# The file is called savetest.spe, which the game never lists, and it is
# deleted once it has been measured.
set -uo pipefail

bin=${1:?usage: test-save.sh <binary>}
level=${2:-levels/level00.spe}

# The picker as well as the writing: pressing down at a save console opens a
# window full of slots before any file is touched, and that window is where
# the Windows crash has to be, since the writing passes there.
out=$("$bin" --headless -nodelay --level "$level" \
      --input-script tests/inputs/level00-run.txt --max-ticks 60 \
      --save-dialog --save-test -datadir ./data 2>&1)

echo "$out" | grep -E "save-test:|save-dialog:|Failed to save|Unable to open file" || true

if ! echo "$out" | grep -q "^save-dialog: returned"; then
    echo "FAIL: the save-slot picker did not come back"
    exit 1
fi

if ! echo "$out" | grep -q "^save-test: wrote"; then
    echo "FAIL: no savegame was written"
    exit 1
fi

if echo "$out" | grep -q "save-test: FAILED"; then
    exit 1
fi

echo "ok: savegame written and removed"
