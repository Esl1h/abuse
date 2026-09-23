#!/usr/bin/env bash
# Records a replay from someone actually playing.
#
# The synthetic replays in tests/replays/ load a level and stand still: they
# exercise loading, activation and the tick loop, and nothing else. No shot
# is fired in any of them, which is why every defect found in combat, in the
# lighting and in the particles was found by a person and not by the suite.
#
#   ./scripts/record-played.sh build/release/src/abuse levels/level00.spe \
#       tests/replays/level00-played.rec
#
# Play, then quit the usual way, with Esc and Quit. The file is written
# when the recording stops, and quitting stops it; pressing Enter stops it
# without leaving the game, which is how the 1995 code has always done it
# and is useful for cutting a session short.
#
# Afterwards, to make it part of the suite:
#
#   ./scripts/update-golden.sh build/dev/src/abuse      # the state hash
#   WINDOW=1 ./scripts/update-golden.sh build/dev/src/abuse
#
# and commit the .rec together with its references, in a commit of its own.
set -euo pipefail

bin=${1:?usage: record-played.sh <binary> <level> <output> [seed]}
level=${2:?level, for example levels/level00.spe}
out=${3:?the .rec file to write}
seed=${4:-1}

mkdir -p "$(dirname "$out")"

# A window, and the gamepad open: the input is the point of the exercise.
# The seed is pinned so the recording can be replayed to the same state.
#
# -preset classic, because a replay is compared against reference frames
# taken that way, and the visual additions are not part of what is being
# recorded.
"$bin" -window -preset classic --seed "$seed" \
       --level "$level" --record "$out" \
       -datadir ./data

printf '\nrecorded %s (%s, seed %s)\n' "$out" "$level" "$seed"
printf 'play it back with:\n'
printf '  %s --headless -nodelay --playback %s --state-hash -datadir ./data\n' \
       "$bin" "$out"
