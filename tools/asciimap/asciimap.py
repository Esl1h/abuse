#!/usr/bin/env python3
"""Compile an ASCII map into an Abuse level.

The point of this tool is the division of labour. A text grid says where the
solid rock is and where the player starts; the compiler decides which of the
game's 1135 foreground tiles to put in each cell, checks that the result can
actually be walked through, and writes it into a real .lvl. Shape is authored,
appearance and correctness are derived.

It does not create a level from nothing. A level carries object tables whose
layout depends on the Lisp that was loaded when it was saved, so this works
from a template: it rewrites the tile maps and moves the objects the template
already has, and leaves every other entry exactly as it found it.

    ./tools/asciimap/asciimap.py check  maps/distress.txt
    ./tools/asciimap/asciimap.py build  maps/distress.txt \
        --template data/netlevel/2play2.spe \
        --out data/addon/challenge/maps/distress.lvl

Which tiles are solid is not guessed: `abuse --dump-tiles` prints the
collision boundary the art itself carries, because this engine has no
hardness table.
"""

import argparse
import collections
import pathlib
import struct
import subprocess
import sys

TOOL = "build/dev/src/abuse-tool"

# Tiles, by the id the game knows them as. Every foreground id here was
# checked with --dump-tiles: the fills and surfaces carry a boundary over the
# whole 30x15 cell, and 0 carries none at all.
#
# Each surface is a small set rather than one tile. A single tile repeated
# over a few thousand cells stops reading as a wall and starts reading as
# wallpaper, which is the first thing that went wrong when this tool drew a
# map with one id per role.
EMPTY = 0
FILL = [11, 10, 27, 28]          # the inside of a mass of rock
FILL_DETAIL = [12, 13, 23, 24]   # machinery, sprinkled into the fill
FLOOR = [37, 38, 43, 44]         # rock with air above it
CEILING = [33, 34, 35, 36]       # rock with air below it

# Background, from art/back/tech.spe, which startup.lsp numbers 110-139.
# Open space over a black background reads as nothing at all, so the level
# needs something behind it to read as a place rather than as a hole.
#
# The ids are the numbers in the entry names, not a position in a list:
# load_tiles reads each entry's name with sscanf and indexes the tile array
# by it, leaving the gaps between sets empty. An id that falls in a gap
# silently draws the black tile.
BACK = [110, 111, 112, 113, 114, 117, 120, 121]

# How much of the background carries structure. The rest is the black tile.
# Measured from the shipped levels: level02's background is 91% empty, and
# filling every cell instead makes the background compete with the
# foreground until a player cannot tell what is solid. Sparse detail over
# black is what reads.
BACK_DENSITY = 12  # per cent

BTILE_W, BTILE_H = 60, 30
TILE_W, TILE_H = 30, 15

# One machinery tile in this many fill cells.
DETAIL_IN = 23


def pick(choices, x, y, salt=0):
    """Deterministic variation.

    Not random: the same map compiles to the same level every time, which is
    what lets a level have a golden frame like everything else here.
    """
    h = (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
    return choices[(h >> 7) % len(choices)]

# What a character in the grid means.
SOLID = "#"
AIR = "."
PLAYER = "P"
MARKS = "PXaEh"  # everything that places an object rather than a tile

JUMP_UP = 3      # tiles the player clears from standing
JUMP_ACROSS = 5


def run(*args):
    out = subprocess.run(args, capture_output=True)
    if out.returncode != 0:
        sys.exit(f"{' '.join(args)}\n{out.stderr.decode(errors='replace')}")
    return out.stdout


def entry_ids(level):
    ids = {}
    for line in run(TOOL, level, "list").decode(errors="replace").splitlines()[2:]:
        parts = line.split()
        if len(parts) >= 5:
            ids[parts[-1]] = parts[0]
    return ids


def read_map(path):
    """Front matter of `key = value`, then `[map]`, then the grid."""
    head, grid, in_grid = {}, [], False
    for raw in pathlib.Path(path).read_text().splitlines():
        if not in_grid:
            if raw.strip() == "[map]":
                in_grid = True
            elif "=" in raw and not raw.startswith(SOLID):
                k, v = raw.split("=", 1)
                head[k.strip()] = v.strip()
            continue
        if raw.strip():
            grid.append(raw.rstrip("\n"))

    if not grid:
        sys.exit(f"{path}: no grid after [map]")

    width = max(len(r) for r in grid)
    grid = [r.ljust(width, AIR) for r in grid]
    return head, grid


def check(grid):
    """Everything that makes a map unplayable rather than ugly."""
    errors = []
    h, w = len(grid), len(grid[0])

    starts = [(x, y) for y in range(h) for x in range(w) if grid[y][x] == PLAYER]
    if len(starts) != 1:
        errors.append(f"expected exactly one '{PLAYER}', found {len(starts)}")

    for y in range(h):
        for x in range(w):
            c = grid[y][x]
            if c not in (SOLID, AIR) and c not in MARKS:
                errors.append(f"unknown character '{c}' at {x},{y}")

    # The level has to be closed, or the player walks out of the world.
    for x in range(w):
        if grid[0][x] != SOLID or grid[h - 1][x] != SOLID:
            errors.append(f"column {x} is open at the top or the bottom")
            break
    for y in range(h):
        if grid[y][0] != SOLID or grid[y][w - 1] != SOLID:
            errors.append(f"row {y} is open at the left or the right")
            break

    if not starts:
        return errors, set()

    # Can the player get there? A coarse model on purpose: it says a cell is
    # reachable if the player can walk, fall or jump into it. It is allowed
    # to be optimistic about a jump; it is not allowed to miss a sealed room,
    # which is the failure that matters.
    def solid(x, y):
        return not (0 <= x < w and 0 <= y < h) or grid[y][x] == SOLID

    open_cells = {(x, y) for y in range(h) for x in range(w) if not solid(x, y)}
    seen, queue = set(starts), collections.deque(starts)
    while queue:
        x, y = queue.popleft()
        moves = [(x - 1, y), (x + 1, y), (x, y + 1)]          # walk, fall
        for dx in range(-JUMP_ACROSS, JUMP_ACROSS + 1):        # jump
            for dy in range(1, JUMP_UP + 1):
                moves.append((x + dx, y - dy))
        for nx, ny in moves:
            if (nx, ny) in open_cells and (nx, ny) not in seen:
                seen.add((nx, ny))
                queue.append((nx, ny))

    orphans = open_cells - seen
    if orphans:
        sx, sy = sorted(orphans)[0]
        errors.append(f"{len(orphans)} open cells are unreachable, "
                      f"first at {sx},{sy}")

    return errors, seen


def autotile(grid):
    """Shape in, appearance out.

    A cell is only ever solid or not in the grid. Which tile it becomes
    depends on its neighbours, so that a mass of rock gets a surface where it
    meets air and plain fill everywhere else. This is the part a person would
    otherwise do 4000 times by hand.
    """
    h, w = len(grid), len(grid[0])
    out = [[EMPTY] * w for _ in range(h)]

    def solid(x, y):
        return not (0 <= x < w and 0 <= y < h) or grid[y][x] == SOLID

    for y in range(h):
        for x in range(w):
            if not solid(x, y):
                continue
            if not solid(x, y - 1):
                out[y][x] = pick(FLOOR, x, y)
            elif not solid(x, y + 1):
                out[y][x] = pick(CEILING, x, y)
            elif pick(range(DETAIL_IN), x, y, 1) == 0:
                out[y][x] = pick(FILL_DETAIL, x, y, 2)
            else:
                out[y][x] = pick(FILL, x, y)
    return out


def background(bw, bh):
    out = []
    for y in range(bh):
        row = []
        for x in range(bw):
            if pick(range(100), x, y, 4) < BACK_DENSITY:
                row.append(pick(BACK, x, y, 3))
            else:
                row.append(EMPTY)
        out.append(row)
    return out


def pack_map(tiles):
    h, w = len(tiles), len(tiles[0])
    body = b"".join(struct.pack("<%dH" % w, *row) for row in tiles)
    return struct.pack("<II", w, h) + body


def build(args):
    head, grid = read_map(args.mapfile)
    errors, _ = check(grid)
    if errors:
        for e in errors:
            print(f"error: {e}", file=sys.stderr)
        sys.exit(1)

    template = pathlib.Path(args.template)
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(template.read_bytes())

    ids = entry_ids(str(out))
    fg_old = run(TOOL, str(out), "get", ids["fgmap"])
    tw, th = struct.unpack("<II", fg_old[:8])
    if (len(grid[0]), len(grid)) != (tw, th):
        sys.exit(f"the grid is {len(grid[0])}x{len(grid)} and the template's "
                 f"map is {tw}x{th}. They must match: the background size and "
                 f"the scroll rate are tuned to it, and this tool does not "
                 f"retune them.")

    tiles = autotile(grid)
    write_entry(str(out), ids["fgmap"], 18, "fgmap", pack_map(tiles))

    # The background is resized to cover the level. It is not a copy of the
    # foreground grid: it scrolls at its own rate, so how much of it is
    # needed depends on that rate, and a background that runs out does not
    # fail, it quietly draws the black tile from there on. That is what made
    # the first version of this map look like a hole with walls around it.
    rate = run(TOOL, str(out), "get", ids["bg_scroll_rate"])
    xmul, xdiv, ymul, ydiv = struct.unpack_from("<4I", rate, 1)
    bw = (len(grid[0]) * TILE_W * xmul // xdiv + 640) // BTILE_W + 2
    bh = (len(grid) * TILE_H * ymul // ydiv + 480) // BTILE_H + 2
    write_entry(str(out), ids["bgmap"], 19, "bgmap", pack_map(background(bw, bh)))
    print(f"background {bw}x{bh} tiles at {xmul}/{xdiv} by {ymul}/{ydiv}")

    place_objects(str(out), ids, grid, head)
    print(f"wrote {out}")


def write_entry(level, entry_id, entry_type, name, payload):
    tmp = pathlib.Path("/tmp") / f"asciimap-{name}.bin"
    tmp.write_bytes(payload)
    run(TOOL, level, "del", entry_id)
    run(TOOL, level, "put", entry_id, str(entry_type), str(tmp))
    run(TOOL, level, "rename", entry_id, name)
    tmp.unlink()


def place_objects(level, ids, grid, head):
    """Move the template's objects onto the new map.

    The types are left alone. An object's variables are stored in a blob
    sized by its type, so changing what something is would desynchronise
    the whole table; moving it is safe and is all this needs.
    """
    tile_w, tile_h = 30, 15
    names = object_names(level, ids)
    types = struct.unpack_from("<%dH" % ((len(x := run(TOOL, level, "get", ids["type"])) - 1) // 2),
                               x, 1)
    n = len(types)

    xs = list(struct.unpack_from("<%di" % n, run(TOOL, level, "get", ids["x"]), 1))
    ys = list(struct.unpack_from("<%di" % n, run(TOOL, level, "get", ids["y"]), 1))

    marks = collections.defaultdict(list)
    for gy, row in enumerate(grid):
        for gx, c in enumerate(row):
            if c in MARKS:
                marks[c].append((gx * tile_w + tile_w // 2,
                                 gy * tile_h + tile_h - 1))

    # Anything with nowhere to go is parked deep inside the rock, where the
    # player cannot reach it and it cannot reach the player.
    #
    # Not on the player's own square, which is what this did first: a
    # template carries solid objects as well as pickups, and 2play2's two
    # doors and its trap door walled the player in on the spot. The level
    # looked right, loaded fine, and could not be walked out of.
    park = (2 * tile_w, 2 * tile_h)

    want = {"START": PLAYER, "HEALTH": "h",
            "PLASMA_ICON20": "a", "LSABER_ICON50": "a"}
    used = collections.Counter()

    for i, t in enumerate(types):
        name = names[t] if t < len(names) else "?"
        mark = want.get(name)
        spots = marks.get(mark, []) if mark else []
        if spots:
            px, py = spots[used[mark] % len(spots)]
            used[mark] += 1
        else:
            px, py = park
        xs[i], ys[i] = px, py

    write_entry(level, ids["x"], 20, "x", b"\x02" + struct.pack("<%di" % n, *xs))
    write_entry(level, ids["y"], 20, "y", b"\x02" + struct.pack("<%di" % n, *ys))
    print(f"placed {n} objects: " +
          ", ".join(f"{k}x{v}" for k, v in used.items()) or "none on a mark")


def object_names(level, ids):
    blob = run(TOOL, level, "get", ids["object_descripitions"])
    total = struct.unpack("<H", blob[:2])[0]
    raw = run(TOOL, level, "get", ids["describe_names"])
    names, i = [], 0
    while i < len(raw) and len(names) < total:
        n = raw[i]
        i += 1
        names.append(raw[i:i + n].split(b"\0")[0].decode("latin-1"))
        i += n
    return names


def cmd_check(args):
    head, grid = read_map(args.mapfile)
    errors, seen = check(grid)
    h, w = len(grid), len(grid[0])
    solid = sum(row.count(SOLID) for row in grid)
    print(f"{args.mapfile}: {w}x{h} tiles, {w * tile_px(0)}x{h * tile_px(1)} pixels")
    print(f"  solid {solid} of {w * h} ({100 * solid // (w * h)}%), "
          f"reachable open cells {len(seen)}")
    for e in errors:
        print(f"error: {e}", file=sys.stderr)
    return 1 if errors else 0


def tile_px(which):
    return (30, 15)[which]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    c = sub.add_parser("check", help="validate a map without writing anything")
    c.add_argument("mapfile")

    b = sub.add_parser("build", help="compile a map into a level")
    b.add_argument("mapfile")
    b.add_argument("--template", required=True)
    b.add_argument("--out", required=True)

    args = ap.parse_args()
    if args.cmd == "check":
        sys.exit(cmd_check(args))
    build(args)


if __name__ == "__main__":
    main()
