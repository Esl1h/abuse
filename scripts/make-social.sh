#!/usr/bin/env bash
# Builds doc/art/social-preview.png, the 1280x640 card GitHub shows when
# somebody shares the repository link.
#
#   ./scripts/make-social.sh
#
# The key art is nearly square, 862x811, and the card is 2:1. Cropping the
# art to that shape cuts the character's head off, measured: scaled to cover
# 1280 wide it stands 1204 tall, and the middle 640 rows start below the
# helmet. So the art is fitted to the height and the sides are filled with
# the same art stretched, blurred and darkened, with the join feathered over
# seventy pixels. The result has no visible seam and loses nothing.
#
# Needs ImageMagick, and only to regenerate: the result is committed.
set -euo pipefail

root=$(cd -- "$(dirname -- "$0")/.." && pwd)
src="$root/doc/art/key-art-wordmark.png"
out="$root/doc/art/social-preview.png"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

command -v magick > /dev/null || { echo "ImageMagick not found" >&2; exit 1; }
[ -f "$src" ] || { echo "missing $src" >&2; exit 1; }

# The filler: the same art stretched to the whole card, blurred past
# recognition and taken down to 45% brightness so it reads as a surround
# rather than as a second picture.
magick "$src" -resize 1280x640! -blur 0x30 -modulate 45 "$tmp/bg.png"

magick "$src" -filter Lanczos -resize x640 "$tmp/fg.png"
width=$(magick identify -format "%w" "$tmp/fg.png")

# A ramp in from each side, so the sharp panel dissolves into the filler.
magick \
    \( -size 640x70 gradient:black-white -rotate 90 \) \
    \( -size "$((width - 140))x640" xc:white \) \
    \( -size 640x70 gradient:white-black -rotate 90 \) \
    +append "$tmp/mask.png"

magick "$tmp/fg.png" "$tmp/mask.png" -alpha off \
    -compose CopyOpacity -composite "$tmp/fg-feathered.png"

magick "$tmp/bg.png" "$tmp/fg-feathered.png" -gravity center \
    -compose over -composite -strip "$out"

echo "$out: $(magick identify -format '%wx%h, %b' "$out")"
