#!/usr/bin/env bash
# Dumps frames at fixed ticks for every replay and compares them pixel for
# pixel with tests/golden/frames/. Exits 77 when there is nothing to compare.
set -euo pipefail

bin=${1:?usage: test-snapshots.sh <binary>}
preset=${PRESET:-classic}
ticks=${TICKS:-100,200,300}

# WINDOW=1 compares what was presented in the window, after scaling, the
# filter and the letterbox bars, instead of the 320x200 buffer the game draws
# into. They are different things: the buffer does not change when the scale
# mode does. Size and preset are pinned here, or the result would depend on
# the machine's abuserc.
window=${WINDOW:-0}
if [ "$window" = 1 ]; then
    golden_dir=tests/golden/window
    glob='*-window.bmp'
    # A fixed window size: the default is fullscreen, which would make the
    # result depend on the monitor's resolution.
    # 800x450 on purpose. It has to be 16:9, so that there are bars, and it
    # has to give a non-integer scale (1.875 here): at an integer scale the
    # letterbox and integer modes produce the same frame and PixelArt looks
    # like Nearest, which would make the test blind to the very options it
    # exists to watch.
    extra=(--dump-window --window-size 800 450 -preset "$preset")
    # PNG references: lossless, and they fit in the repository. The 640x480
    # BMP is 900 KB and the PNG is 16 KB.
    ref_ext=png
    ticks=${TICKS:-200}
else
    golden_dir=tests/golden/frames
    glob='*[0-9].bmp'
    extra=()
    ref_ext=bmp
fi

command -v compare > /dev/null || { echo "ImageMagick 'compare' is missing"; exit 77; }

shopt -s nullglob
recs=(tests/replays/*.rec)
if [ ${#recs[@]} -eq 0 ]; then
    echo "no replays in tests/replays/, nothing to check"
    exit 77
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
rc=0
compared=0

for rec in "${recs[@]}"; do
    name=$(basename "$rec" .rec)
    mkdir -p "$tmp/$name"

    "$bin" --headless -nodelay --playback "$rec" \
           --dump-frames "$ticks" --out "$tmp/$name" "${extra[@]}" \
           -datadir ./data > /dev/null 2>&1

    # shellcheck disable=SC2086 # $glob is a pattern, it is meant to expand
    for f in "$tmp/$name"/$glob; do
        ref="$golden_dir/$name/$preset/$(basename "$f" .bmp).$ref_ext"
        if [ ! -f "$ref" ]; then
            echo "FAIL: $name/$(basename "$f") has no reference"
            rc=1
            continue
        fi
        # ImageMagick 7 prints "0 (0)"; keep the absolute count only.
        ae=$(compare -metric AE "$f" "$ref" null: 2>&1 || true)
        ae=${ae%% *}
        compared=$((compared + 1))
        if [ "$ae" != "0" ]; then
            echo "DIFFERS $name/$(basename "$f"): AE=$ae"
            rc=1
        else
            echo "ok: $name/$(basename "$f")"
        fi
    done
done

# The new screens are drawn into the native resolution overlay, which only
# exists after scaling: neither suite above would see them without the
# --dump-* flags. Each shot carries the arguments it needs, from the fifth
# field on.
if [ "$window" = 1 ]; then
    rec=tests/replays/level00-idle.rec
    if [ -f "$rec" ]; then
        for shot in \
            "options-en 1280 720 en --dump-options" \
            "options-pt 1024 600 pt_BR --dump-options" \
            "controls-en 1280 720 en --dump-controls" \
            "classic-en 1280 720 en --dump-classic-data --mode original --classic-data /opt/abuse/classic" \
            "language-en 1280 720 en --dump-language" \
            "menuhint-en 1280 720 en --dump-menu-hint" \
            "startmenu-en 1280 720 en --dump-start-menu" \
            "startmenu-pt 1024 600 pt_BR --dump-start-menu" \
            "hud-en 1280 720 en --dump-hud" \
            "rgblight-en 1280 720 en --rgb-light" \
            "crt-en 1280 720 en --rgb-light --scanlines" \
            "interp-en 1280 720 en --frame-alpha 0.5 --replay level00-run" \
            "particles-en 1280 720 en --particle-demo" \
            "dynlight-en 1280 720 en --rgb-light --dynlight-demo"
        do
            # shellcheck disable=SC2086 # the fields are meant to split
            set -- $shot
            name=$1
            win_w=$2
            win_h=$3
            lang=$4
            shift 4

            # --replay <name> picks a different recording for this shot. The
            # default is the idle one, which is right for a screen drawn over
            # a still frame and wrong for anything about movement.
            shot_rec=$rec
            args=()
            while [ $# -gt 0 ]; do
                if [ "$1" = "--replay" ]; then
                    shot_rec="tests/replays/$2.rec"
                    shift 2
                else
                    args+=("$1")
                    shift
                fi
            done

            mkdir -p "$tmp/$name"
            "$bin" --headless -nodelay --playback "$shot_rec" \
                   --dump-frames 200 --out "$tmp/$name" \
                   --dump-window --window-size "$win_w" "$win_h" \
                   -preset "$preset" -language "$lang" "${args[@]}" \
                   -datadir ./data > /dev/null 2>&1

            f="$tmp/$name/000200-window.bmp"
            ref="$golden_dir/$name/$preset/000200-window.png"
            if [ ! -f "$f" ] || [ ! -f "$ref" ]; then
                echo "FAIL: $name has no frame or no reference"
                rc=1
                continue
            fi
            ae=$(compare -metric AE "$f" "$ref" null: 2>&1 || true)
            ae=${ae%% *}
            compared=$((compared + 1))
            if [ "$ae" != "0" ]; then
                echo "DIFFERS $name: AE=$ae"
                rc=1
            else
                echo "ok: $name"
            fi
        done
    fi
fi

[ "$compared" -eq 0 ] && { echo "no frames compared"; exit 77; }
exit "$rc"
