#!/usr/bin/env bash
# Regenerates the reference hashes and frames. Intentional changes only, and
# in a commit of its own.
set -euo pipefail

bin=${1:?usage: update-golden.sh <binary>}
mode=${2:-original}
preset=${PRESET:-classic}
ticks=${TICKS:-100,200,300}

# See test-snapshots.sh: WINDOW=1 records what the window presented.
window=${WINDOW:-0}
if [ "$window" = 1 ]; then
    golden_dir=tests/golden/window
    glob='*-window.bmp'
    extra=(--dump-window --window-size 800 450 -preset "$preset")
    ticks=${TICKS:-200}
else
    golden_dir=tests/golden/frames
    glob='*[0-9].bmp'
    extra=()
fi

shopt -s nullglob
recs=(tests/replays/*.rec)
[ ${#recs[@]} -eq 0 ] && { echo "no replays in tests/replays/"; exit 1; }

mkdir -p "tests/golden/hash"

for rec in "${recs[@]}"; do
    name=$(basename "$rec" .rec)

    "$bin" --headless -nodelay --playback "$rec" --state-hash -datadir ./data 2>/dev/null \
        | grep '^final' > "tests/golden/hash/$name.$mode.hash"
    echo "hash:   tests/golden/hash/$name.$mode.hash"

    out="$golden_dir/$name/$preset"
    mkdir -p "$out"
    rm -f "$out"/*.bmp "$out"/*.png
    tmp=$(mktemp -d)
    "$bin" --headless -nodelay --playback "$rec" \
           --dump-frames "$ticks" --out "$tmp" "${extra[@]}" \
           -datadir ./data > /dev/null 2>&1
    # shellcheck disable=SC2086 # $glob is a pattern, it is meant to expand
    for f in "$tmp"/$glob; do
        if [ "$window" = 1 ]; then
            # PNG is lossless, so the comparison stays exact.
            magick "$f" "$out/$(basename "$f" .bmp).png"
        else
            cp "$f" "$out/"
        fi
    done
    rm -rf "$tmp"
    echo "frames: $out"
done

# See test-snapshots.sh: the captures of the new screens.
if [ "$window" = 1 ]; then
    rec=tests/replays/level00-idle.rec
    for shot in \
        "options-en 1280 720 en --dump-options" \
        "options-pt 1024 600 pt_BR --dump-options" \
        "controls-en 1280 720 en --dump-controls" \
        "classic-en 1280 720 en --dump-classic-data --mode original --classic-data /opt/abuse/classic" \
        "language-en 1280 720 en --dump-language" \
        "menuhint-en 1280 720 en --dump-menu-hint" \
        "startmenu-en 1280 720 en --dump-start-menu" \
        "startmenu-pt 1024 600 pt_BR --dump-start-menu" \
        "hud-en 1280 720 en --dump-hud"
    do
        # shellcheck disable=SC2086 # the fields are meant to split
        set -- $shot
        name=$1
        win_w=$2
        win_h=$3
        lang=$4
        shift 4

        out="$golden_dir/$name/$preset"
        mkdir -p "$out"
        rm -f "$out"/*.png
        tmp=$(mktemp -d)
        "$bin" --headless -nodelay --playback "$rec" \
               --dump-frames 200 --out "$tmp" \
               --dump-window --window-size "$win_w" "$win_h" \
               -preset "$preset" -language "$lang" "$@" \
               -datadir ./data > /dev/null 2>&1
        magick "$tmp/000200-window.bmp" "$out/000200-window.png"
        rm -rf "$tmp"
        echo "screen: $out"
    done
fi
