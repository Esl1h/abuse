#!/usr/bin/env bash
# The GPU presentation path, compared against the references the classic one
# produced.
#
# Why this exists: on 2026-09-22 three defects shipped in that path and all
# three were found by a person playing, not here. The overlay was invisible,
# the pointer was in the wrong coordinates, and the scale it was multiplied
# by was zero. They have one cause between them, that the GPU path has no
# SDL_Renderer and the code asked it anyway, and the window suite could not
# see any of it because it runs the classic renderer on purpose: that is
# where the reference frames come from, and a CI runner has no Vulkan.
#
# So this compares the two paths against *each other*, in the same session,
# rather than against the stored references. It has to: a window read back
# from a real driver does not match one read back from SDL's dummy one, by
# a lot. Measured on the classic renderer alone, same frame, same options,
# the windowed capture came out with a mean of 74.9 against the golden's
# 14.9, which is what a linear-versus-sRGB readback looks like. The stored
# references are all headless, deliberately, and are not comparable here.
#
# What is comparable, and is the invariant that broke, is that the two
# paths draw the same picture as each other.
#
# Exits 77 (CTest's skip code) when the GPU path cannot start, which is the
# normal answer on a machine with no Vulkan and in CI. That is not a pass
# dressed up as a skip: a machine that can run it does.
set -uo pipefail

bin=${1:?usage: test-gpu.sh <binary>}
rec=tests/replays/level00-idle.rec

[ -f "$rec" ] || { echo "no replay at $rec, nothing to check"; exit 77; }

command -v compare > /dev/null 2>&1 || {
    echo "ImageMagick's compare is not installed, skipping"
    exit 77
}

# One run of the game, bounded, for the same reason the other suites bound
# theirs: a stuck process should name itself rather than eat the budget.
bound=""
if command -v timeout > /dev/null 2>&1; then
    bound=${ABUSE_RUN_TIMEOUT:-120}
fi

run_game() {
    if [ -n "$bound" ]; then
        timeout "$bound" "$@"
    else
        "$@"
    fi
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# A home of its own, so the run does not read the abuserc of whoever is
# sitting at this machine. Without it the comparison would depend on their
# aspect, filter and HUD settings, and a first run with no language
# configured draws the language screen over everything.
home="$tmp/home"
for mode in remaster original; do
    mkdir -p "$home/.config/abuse/$mode"
    printf 'language=en\n' > "$home/.config/abuse/$mode/abuserc"
done

shot_to() {
    local out=$1 renderer=$2
    shift 2
    mkdir -p "$out"
    # --dump-tick and not --dump-frames: frames are counted by the loop,
    # which in a windowed run means once per drawn frame, so the same frame
    # number is a different moment on a faster path. A level tick is the
    # same moment everywhere, and with interpolation off every frame drawn
    # inside one tick is the same picture. The run ends once the capture is
    # taken.
    #
    # -filter nearest, because the two paths scale differently by design.
    # The classic one asks SDL for SDL_SCALEMODE_PIXELART and this one runs
    # a sharp-bilinear in its shader; they are meant to look alike, not to
    # be the same arithmetic. Nearest is the one filter where both have to
    # produce exactly the same pixels, so it is the one that can be
    # compared without a tolerance that would hide a real difference.
    HOME="$home" run_game "$bin" -nodelay --renderer "$renderer" \
        --playback "$rec" --dump-tick 20 --dump-window \
        --window-size 1280 720 "$@" --out "$out" -preset classic \
        -filter nearest -novsync -language en -datadir ./data > /dev/null 2>&1
}

# Does this machine have the path at all? A window and a Vulkan device are
# both needed, and neither exists in CI.
probe=$(HOME="$home" "$bin" --renderer gpu --playback "$rec" --max-ticks 1 \
        -nodelay -language en -datadir ./data 2>&1 \
        | grep -c "presenting through SDL_GPU" || true)
if [ "$probe" -eq 0 ]; then
    echo "the GPU path did not start here, skipping"
    exit 77
fi

rc=0
compared=0

# The three screens drawn into the native resolution overlay, which is what
# broke, plus the plain frame, which is the picture itself.
for shot in "options --dump-options" \
            "startmenu --dump-start-menu" \
            "hud --dump-hud" \
            "plain"
do
    # shellcheck disable=SC2086 # the fields are meant to split
    set -- $shot
    name=$1
    shift

    shot_to "$tmp/$name-classic" classic "$@"
    shot_to "$tmp/$name-gpu" gpu "$@"

    # Named after the frame number, which is not the same on the two
    # paths and is not meant to be: what was pinned is the tick.
    a=$(find "$tmp/$name-classic" -name '*-window.bmp' | head -1)
    b=$(find "$tmp/$name-gpu" -name '*-window.bmp' | head -1)
    if [ -z "$a" ] || [ -z "$b" ]; then
        echo "FAIL: $name produced no frame on one of the two paths"
        rc=1
        continue
    fi

    ae=$(compare -metric AE "$a" "$b" null: 2>&1 || true)
    ae=${ae%% *}
    compared=$((compared + 1))
    if [ "$ae" != "0" ]; then
        echo "DIFFERS $name: AE=$ae between the classic and the GPU path"
        rc=1
    else
        echo "ok: $name is the same picture on both paths"
    fi
done

if [ "$compared" -eq 0 ]; then
    echo "nothing was compared"
    exit 1
fi

exit "$rc"
