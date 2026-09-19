#!/usr/bin/env bash
# Looks for level edges that a wider view would leave empty.
#
# The levels were drawn for a 320 by 200 view. A wider one shows more of the
# world, and where the world was never drawn it shows black. This walks every
# level at each aspect ratio and measures how much light there is in a strip
# down each side of the frame: a strip that is black is a place where the art
# ran out.
#
# It is a first pass, not a proof. Each level is sampled at one position, the
# one a short input script reaches, and a level can be fine there and empty
# somewhere else. What it can do is say "this one needs looking at" without
# anybody looking at twenty-two of them.
#
# Exits 77 (CTest's skip code) when ImageMagick is missing, like the snapshot
# suites, and 1 when a strip comes out under the threshold.
set -uo pipefail

bin=${1:?usage: scan-levels.sh <binary> [aspect...]}
shift
aspects=("$@")
[ ${#aspects[@]} -eq 0 ] && aspects=(427 560)

command -v magick > /dev/null || { echo "ImageMagick not found"; exit 77; }

# Under this much light in a 25 pixel strip, call it empty. Abuse is a dark
# game: 1% is already darker than any corner of it that has art in it.
threshold=${THRESHOLD:-1.0}

script=${SCRIPT:-tests/inputs/level00-run.txt}
[ -f "$script" ] || { echo "no input script at $script"; exit 1; }

rc=0
printf '%-10s %-6s %-9s %-9s\n' level view left right

for level in data/levels/level*.spe; do
    name=$(basename "$level" .spe)

    for width in "${aspects[@]}"; do
        tmp=$(mktemp -d)
        "$bin" --headless -nodelay --level "levels/$name.spe" \
               --input-script "$script" --max-ticks 120 \
               --dump-frames 110 --out "$tmp" \
               --viewport "$width" 200 -datadir ./data > /dev/null 2>&1

        frame="$tmp/000110.bmp"
        if [ ! -f "$frame" ]; then
            printf '%-10s %-6s %s\n' "$name" "$width" "no frame"
            rc=1
            rm -rf "$tmp"
            continue
        fi

        # A strip down each side, clear of the status bar at the bottom.
        left=$(magick "$frame" -crop 25x140+0+25 +repage -colorspace gray \
                      -format "%[fx:mean*100]" info:)
        right=$(magick "$frame" -crop "25x140+$((width - 25))+25" +repage \
                      -colorspace gray -format "%[fx:mean*100]" info:)
        rm -rf "$tmp"

        flag=""
        if awk -v l="$left" -v r="$right" -v t="$threshold" \
               'BEGIN { exit !(l < t || r < t) }'; then
            flag=" <- empty edge"
            rc=1
        fi

        printf '%-10s %-6s %-9.2f %-9.2f%s\n' \
               "$name" "$width" "$left" "$right" "$flag"
    done
done

exit $rc
