#!/usr/bin/env bash
# The simulation must not depend on how wide the window is.
#
# The levels were drawn for a 4:3 view, and what wakes up in one is decided
# by the region around the player, not by the window: a wider view shows more
# of the room and has to fight exactly the same fight. If that ever stops
# being true, widescreen becomes a different game and every reference hash
# recorded at 4:3 becomes a lie.
#
# Exits 77 (CTest's skip code) when there is no replay to run.
set -uo pipefail

bin=${1:?usage: test-aspect.sh <binary>}

shopt -s nullglob

# One replay by default, and the one with movement in it: four widths times
# every recording is four minutes of suite for coverage that barely differs,
# since a player standing still activates the same things wherever the edges
# of the screen are. ASPECT_ALL=1 runs the lot.
if [ -n "${ASPECT_ALL:-}" ]; then
    recs=(tests/replays/*.rec)
else
    recs=(tests/replays/level00-run.rec)
    [ -f "${recs[0]}" ] || recs=(tests/replays/*.rec)
fi

if [ ${#recs[@]} -eq 0 ]; then
    echo "no replays in tests/replays/, nothing to check"
    exit 77
fi

# 4:3, 16:10, 16:9 and 21:9 of a 200 pixel tall buffer.
widths=(320 400 427 560)

rc=0
for rec in "${recs[@]}"; do
    name=$(basename "$rec" .rec)
    seen=""
    for w in "${widths[@]}"; do
        hash=$("$bin" --headless -nodelay --playback "$rec" --state-hash \
               --viewport "$w" 200 -datadir ./data 2>/dev/null \
               | grep '^final' | grep -o 'hash=[0-9a-f]*')
        if [ -z "$hash" ]; then
            echo "FAIL: $name at ${w}x200 produced no hash"
            rc=1
            continue
        fi
        seen="$seen$w:$hash"$'\n'
    done

    different=$(printf '%s' "$seen" | cut -d: -f2- | sort -u | wc -l)
    if [ "$different" -eq 1 ]; then
        echo "ok: $name, same state at every width"
    else
        echo "FAIL: $name plays differently depending on the window"
        printf '%s' "$seen" | sed 's/^/  /'
        rc=1
    fi
done

exit $rc
