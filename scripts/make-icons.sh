#!/usr/bin/env bash
# Derives every icon the packages need from the one master.
#
# The master is data/freedesktop/icon-source-1024.png, 1024x1024 with a
# transparent background. Everything else in the tree is made from it by
# this script, so there is one file to edit and one command to run rather
# than eight files to keep in step by hand.
#
#   ./scripts/make-icons.sh
#
# Needs ImageMagick, and only to regenerate: the results are committed, so
# nobody needs it to build or to package.
set -euo pipefail

root=$(cd -- "$(dirname -- "$0")/.." && pwd)
app_id=io.github.Esl1h.AbuseVrenna
master="$root/data/freedesktop/icon-source-1024.png"

command -v magick > /dev/null || { echo "ImageMagick not found" >&2; exit 1; }
[ -f "$master" ] || { echo "missing $master" >&2; exit 1; }

# Lanczos rather than the default: the art is a painted poster with fine
# smoke, and a box filter turns that into grey mud below 128.
for size in 64 128 256 512; do
    dir="$root/data/freedesktop/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$dir"
    magick "$master" -filter Lanczos -resize "${size}x${size}" \
        -strip "$dir/$app_id.png"
    echo "  ${size}x${size}"
done

# The Windows executable and the WiX installer both point at this one, and
# an .ico carries every size in a single file. 256 is the one Explorer
# shows on the desktop, 16 the one in the title bar.
magick "$master" -filter Lanczos \
    \( -clone 0 -resize 256x256 \) \
    \( -clone 0 -resize 48x48 \) \
    \( -clone 0 -resize 32x32 \) \
    \( -clone 0 -resize 16x16 \) \
    -delete 0 -strip "$root/doc/icon.ico"
echo "  doc/icon.ico"
