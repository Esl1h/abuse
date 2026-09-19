#!/usr/bin/env bash
# Replays every tests/replays/*.rec and compares the state hash with the
# reference in tests/golden/hash/. Exits 77 (CTest's skip code) when there is
# nothing to replay.
set -euo pipefail

bin=${1:?usage: test-replays.sh <binary>}
mode=${2:-original}

shopt -s nullglob
recs=(tests/replays/*.rec)
if [ ${#recs[@]} -eq 0 ]; then
    echo "no replays in tests/replays/, nothing to check"
    exit 77
fi

rc=0
for rec in "${recs[@]}"; do
    name=$(basename "$rec" .rec)
    golden="tests/golden/hash/$name.$mode.hash"

    out=$("$bin" --headless -nodelay --playback "$rec" --state-hash -datadir ./data 2>/dev/null \
          | grep '^final' || true)

    if [ -z "$out" ]; then
        echo "FAIL: $name ($mode) produced no hash"
        rc=1
        continue
    fi

    if [ ! -f "$golden" ]; then
        echo "FAIL: $name ($mode) has no reference in $golden"
        echo "      run scripts/update-golden.sh to create it"
        rc=1
        continue
    fi

    if [ "$out" != "$(cat "$golden")" ]; then
        echo "FAIL: $name ($mode)"
        echo "  expected: $(cat "$golden")"
        echo "  got:      $out"
        rc=1
    else
        echo "ok: $name ($mode)"
    fi
done

exit "$rc"
