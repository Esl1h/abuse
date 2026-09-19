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

errors=$(mktemp)
trap 'rm -f "$errors"' EXIT

rc=0
for rec in "${recs[@]}"; do
    name=$(basename "$rec" .rec)
    golden="tests/golden/hash/$name.$mode.hash"

    set +e
    raw=$("$bin" --headless -nodelay --playback "$rec" --state-hash -datadir ./data 2>"$errors")
    status=$?
    set -e
    out=$(printf '%s\n' "$raw" | grep '^final' || true)

    if [ -z "$out" ]; then
        # Say why. A suite that hides the reason for failing costs more time
        # than it saves, and this one first failed on a platform none of us
        # can run it on.
        echo "FAIL: $name ($mode) produced no hash (exit $status)"
        echo "  $bin --headless -nodelay --playback $rec --state-hash -datadir ./data"
        printf '%s\n' "$raw" | tail -n 5 | sed 's/^/  out| /'
        tail -n 10 "$errors" | sed 's/^/  err| /'
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
