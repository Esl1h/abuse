#!/usr/bin/env python3
"""Learn how the 1995 levels are tiled, so a generated map can be tiled the
same way.

The first version of the compiler picked a tile at random from a small set
per role, and a person who played the result said what the measurements
could not: the tiles are not interchangeable textures. They are pieces of
larger compositions, with a left edge, a middle, a corner, a machine that
spans several cells. Scattering them produces wall, not a place.

So this does not guess. It reads the levels the artists built, and for every
cell records what tile they used and what the cell's surroundings looked
like. The surroundings are the 3x3 pattern of solid and open around it,
which is exactly what a generated grid also knows. Compiling then becomes a
question with an answer in the data: in a cell that looks like this, what
did they put?

Adjacency is recorded as well, so that between two tiles the originals both
used in a context, the one that actually followed the neighbour to its left
or above is preferred. That is what keeps a run of panels reading as one
composition.

    ./tools/asciimap/learn.py --out tools/asciimap/tiling.json

Solidity is not guessed either: `abuse --dump-tiles` prints the collision
boundary each tile's own art carries, because this engine has no hardness
table.
"""

import argparse
import collections
import json
import pathlib
import struct
import subprocess
import sys

TOOL = "build/dev/src/abuse-tool"
GAME = "build/dev/src/abuse"


def run(*args):
    out = subprocess.run(args, capture_output=True)
    if out.returncode != 0:
        sys.exit(f"{' '.join(args)}\n{out.stderr.decode(errors='replace')}")
    return out.stdout


def tile_solidity():
    """Which foreground tiles block, straight from the loaded art."""
    out = run(GAME, "--headless", "--level", "levels/level00.spe",
              "--dump-tiles", "-datadir", "./data").decode(errors="replace")
    solid = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) != 8 or parts[0] != "tile-dump" or not parts[1].isdigit():
            continue
        solid[int(parts[1])] = int(parts[2]) > 0
    if not solid:
        sys.exit("no tiles reported; is the dev build current?")
    return solid


def read_map(level, which):
    ids = {}
    for line in run(TOOL, level, "list").decode(errors="replace").splitlines()[2:]:
        parts = line.split()
        if len(parts) >= 5:
            ids[parts[-1]] = parts[0]
    if which not in ids:
        return None
    blob = run(TOOL, level, "get", ids[which])
    w, h = struct.unpack("<II", blob[:8])
    cells = struct.unpack("<%dH" % (w * h), blob[8:8 + w * h * 2])
    # The top two bits are flags: 0x4000 draws the tile in front of the
    # player and 0x8000 marks it as seen. Neither is part of the identity.
    return w, h, [c & 0x3FFF for c in cells]


def context(is_solid, w, h, x, y):
    """The 3x3 pattern around a cell, as nine bits.

    Outside the level counts as solid, which is what the compiler assumes
    too: a level is a closed box.
    """
    bits = 0
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            nx, ny = x + dx, y + dy
            bits <<= 1
            if not (0 <= nx < w and 0 <= ny < h) or is_solid(nx, ny):
                bits |= 1
    return bits


def learn_background(levels, size=3, keep=40):
    """Whole blocks of background, lifted from the originals.

    The background has compositions too: a machine spans several cells and
    only reads as a machine when its pieces are together and in order.
    Scattering single background tiles produced fragments floating in the
    dark, which is the same mistake as the foreground and looked worse,
    because there is no shape around them to explain it.

    So this takes complete NxN blocks out of the shipped levels and the
    compiler stamps them whole.
    """
    seen = collections.Counter()
    for level in levels:
        got = read_map(level, "bgmap")
        if not got:
            continue
        w, h, cells = got
        for y in range(h - size + 1):
            for x in range(w - size + 1):
                block = tuple(cells[(y + dy) * w + x + dx]
                              for dy in range(size) for dx in range(size))
                # Only blocks that are entirely filled: a block with a hole
                # in it is the edge of a composition, and stamping it
                # somewhere else puts the edge in the wrong place.
                if all(block):
                    seen[block] += 1
    return [list(b) for b, _ in seen.most_common(keep)]


def learn(levels, solid):
    by_context = collections.defaultdict(collections.Counter)
    right_of = collections.defaultdict(collections.Counter)
    below = collections.defaultdict(collections.Counter)
    seen = 0

    for level in levels:
        got = read_map(level, "fgmap")
        if not got:
            continue
        w, h, cells = got

        def at(x, y):
            return cells[y * w + x]

        def is_solid(x, y):
            return solid.get(at(x, y), False)

        for y in range(h):
            for x in range(w):
                t = at(x, y)
                by_context[context(is_solid, w, h, x, y)][t] += 1
                if x + 1 < w:
                    right_of[t][at(x + 1, y)] += 1
                if y + 1 < h:
                    below[t][at(x, y + 1)] += 1
                seen += 1
        print(f"  {pathlib.Path(level).name}: {w}x{h}")

    return by_context, right_of, below, seen


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--levels", default="data/levels",
                    help="directory of .spe levels to learn from")
    ap.add_argument("--out", default="tools/asciimap/tiling.json")
    ap.add_argument("--keep", type=int, default=6,
                    help="how many tiles to keep per context, most used first")
    args = ap.parse_args()

    solid = tile_solidity()
    print(f"{sum(solid.values())} of {len(solid)} foreground tiles are solid")

    levels = sorted(str(p) for p in pathlib.Path(args.levels).glob("*.spe"))
    if not levels:
        sys.exit(f"no levels in {args.levels}")
    print(f"learning from {len(levels)} levels:")

    by_context, right_of, below, seen = learn(levels, solid)

    patches = learn_background(levels)
    print(f"{len(patches)} background blocks of 3x3")

    model = {
        "cells": seen,
        "bg_patch": patches,
        "solid": sorted(t for t, s in solid.items() if s),
        "context": {str(k): [t for t, _ in v.most_common(args.keep)]
                    for k, v in by_context.items()},
        "right_of": {str(k): [t for t, _ in v.most_common(args.keep)]
                     for k, v in right_of.items()},
        "below": {str(k): [t for t, _ in v.most_common(args.keep)]
                  for k, v in below.items()},
    }

    out = pathlib.Path(args.out)
    out.write_text(json.dumps(model, separators=(",", ":"), sort_keys=True))
    print(f"{seen} cells, {len(by_context)} distinct surroundings")
    print(f"wrote {out} ({out.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
