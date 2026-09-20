#!/usr/bin/env bash
#
# Builds an AppImage of Abuse: Vrenna with linuxdeploy.
#
# NOT YET RUN. linuxdeploy and its GTK/Qt-free plugin set are downloaded on
# demand, which means this script needs the network and does not belong in
# the offline build. It is here so that the identity and the layout are
# settled; the first real run belongs to phase 8.
#
#   ./packaging/appimage/build-appimage.sh
#
# Output: Abuse_Vrenna-<version>-x86_64.AppImage in the repository root.

set -euo pipefail

here=$(cd -- "$(dirname -- "$0")" && pwd)
root=$(cd -- "$here/../.." && pwd)

app_id=io.github.Esl1h.AbuseVrenna
build=$root/build/appimage
appdir=$build/AppDir

command -v linuxdeploy-x86_64.AppImage >/dev/null 2>&1 || {
    echo "linuxdeploy-x86_64.AppImage is not on PATH." >&2
    echo "Get it from https://github.com/linuxdeploy/linuxdeploy/releases" >&2
    exit 1
}

rm -rf "$appdir"

cmake -S "$root" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCPM_USE_LOCAL_PACKAGES=ON
cmake --build "$build"
DESTDIR="$appdir" cmake --install "$build"

install -Dm755 "$root/scripts/fetch-classic-data.sh" \
    "$appdir/usr/bin/abuse-vrenna-fetch-classic-data"

linuxdeploy-x86_64.AppImage \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/abuse-vrenna" \
    --desktop-file "$root/packaging/flatpak/$app_id.desktop" \
    --icon-file "$root/doc/abuse.png" \
    --output appimage

echo "AppImage written to $root"
