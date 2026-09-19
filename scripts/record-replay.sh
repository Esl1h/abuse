#!/usr/bin/env bash
# Records a synthetic replay: a level loaded headless with no input, for a
# fixed number of ticks. These exercise level loading, object activation and
# the tick loop, but no combat: a replay played by a human is worth more.
set -euo pipefail

bin=${1:?usage: record-replay.sh <binary> <level> <output> [ticks] [seed]}
level=${2:?level, for example levels/level00.spe}
out=${3:?the .rec file to write}
ticks=${4:-400}
seed=${5:-1}

mkdir -p "$(dirname "$out")"

"$bin" --headless -nodelay --seed "$seed" --level "$level" \
       --record "$out" --max-ticks "$ticks" \
       -datadir ./data > /dev/null 2>&1

printf 'recorded %s (%s, %s ticks, seed %s)\n' "$out" "$level" "$ticks" "$seed"
