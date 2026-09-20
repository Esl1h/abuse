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
import json
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

# Which tile sets belong together, by the ranges lisp/startup.lsp gives
# them. The artists worked a region in one set, which is why the shipped
# levels read as places: a corridor is built of corridor pieces all the way
# along. Learning from every level at once and stamping whatever fits put a
# city skyline and a cave wall inside a spaceship.
#
# A map picks a theme and the compiler will not leave it.
THEMES = {
    # foregrnd, techno, techno2, techno3, techno4
    "techno": {"fg": [(0, 0), (1, 99), (100, 167), (200, 236), (300, 460)],
               "bg": [(110, 139)]},
    "cave":   {"fg": [(0, 0), (500, 634)], "bg": [(84, 103)]},
    "alien":  {"fg": [(0, 0), (700, 774)], "bg": [(150, 179)]},
    "trees":  {"fg": [(0, 0), (800, 931), (1100, 1134)],
               "bg": [(200, 268)]},
}


def in_theme(tile, ranges):
    return any(lo <= tile <= hi for lo, hi in ranges)

# One machinery tile in this many fill cells. Only used where the
# learned model has nothing to say.
DETAIL_IN = 23

# How far below the best score a tile may be and still be considered.
#
# Zero, after trying 5. Loosening it does not produce the variety the
# original levels have: it produces a different tile per cell, the motifs
# do not line up, and a wall of panels turns into horizontal banding. The
# variety in the originals comes from the shape of the rock, which changes
# the context and therefore the answer, and from the artists changing tile
# set by region, which is a decision and not a jitter.
TIE_MARGIN = 0


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
# Marks place an object instead of choosing a tile. What each one becomes
# depends on the template: the compiler never invents an object, it moves
# one the template already carries, because an object's variables live in a
# blob sized by its type and changing what something is would desynchronise
# the whole table.
#
# So a mark is a request, filled from the pool if the template has something
# for it. A template with no enemies produces a map with no enemies, which
# is what the deathmatch one did.
MARKS = "PXaehct"

ROLES = {
    "P": ["START"],
    "X": ["NEXT_LEVEL", "NEXT_LEVEL_TOP"],
    # On a floor: things that walk.
    "e": ["ROB1", "JUGGER", "ANT", "ANT_TOUGH", "WALK_ROB", "DARNEL"],
    # Under a ceiling: things that come down from it.
    "c": ["ANT_ROOF", "HIDDEN_ANT", "FLYER", "GREEN_FLYER"],
    # Against a wall: things that are mounted on one.
    "t": ["SPRAY_GUN", "TRACK_GUN"],
    "a": ["MBULLET_ICON20", "MBULLET_ICON5", "GRENADE_ICON10",
          "GRENADE_ICON2", "ROCKET_ICON2", "PLASMA_ICON20", "LSABER_ICON50"],
    "h": ["HEALTH", "MEDKIT"],
}

# Measured with --dump-player, not assumed: a standing jump rises 48 pixels
# from the floor, which is 3.2 tiles of 15, and carries about 4.4 tiles
# across. The first version of this file guessed, and the map it passed had
# ledges six and seven rows up that nobody could climb.
JUMP_UP = 3
JUMP_ACROSS = 4


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

    # The same graph, walked backwards from the exit: which cells can still
    # reach it. Anything the player can get to but cannot get out of is a
    # trap, and that is the failure that actually ruins a map.
    #
    # Asking only "can the player get here" is not enough, and a version of
    # this check that asked only that passed a map with ledges six rows up:
    # falling in from above made them reachable, and the tool was happy
    # with a platform nobody could climb.
    exits = [(x, y) for y in range(h) for x in range(w) if grid[y][x] == "X"]
    escapes = set(exits)
    queue = collections.deque(exits)
    while queue:
        x, y = queue.popleft()
        # Who could have moved into this cell: someone beside it, someone
        # above it who fell, or someone below and to the side who jumped.
        back = [(x - 1, y), (x + 1, y), (x, y - 1)]
        for dx in range(-JUMP_ACROSS, JUMP_ACROSS + 1):
            for dy in range(1, JUMP_UP + 1):
                back.append((x + dx, y + dy))
        for nx, ny in back:
            if (nx, ny) in open_cells and (nx, ny) not in escapes:
                escapes.add((nx, ny))
                queue.append((nx, ny))

    trapped = (seen & open_cells) - escapes if exits else set()
    if trapped:
        errors.append(f"{len(trapped)} cells the player can reach but "
                      f"cannot leave: " +
                      ", ".join(f"{x},{y}" for x, y in clusters(trapped, most=5)))

    return errors, seen, trapped


def load_model(path="tools/asciimap/tiling.json"):
    """What the 1995 artists did, as data. See tools/asciimap/learn.py."""
    p = pathlib.Path(path)
    if not p.exists():
        return None
    m = json.loads(p.read_text())
    return {
        "bg_patch": m.get("bg_patch", []),
        "context": {int(k): v for k, v in m["context"].items()},
        "right_of": {int(k): v for k, v in m["right_of"].items()},
        "below": {int(k): v for k, v in m["below"].items()},
    }


def autotile(grid, model, theme):
    """Shape in, appearance out.

    A cell is only ever solid or not in the grid. What tile it becomes is
    answered from the shipped levels: for the 3x3 pattern of solid and open
    around this cell, which tiles did the artists use in a cell that looked
    the same?

    Picking at random from a set of "wall tiles" was the first version, and
    it was wrong in a way no measurement here caught. The tiles are not
    interchangeable textures; they are pieces of larger compositions, with
    edges and corners and machines that span several cells. A person who
    played the result said so, and this is the answer to it.

    Between the tiles a context allows, the one that actually followed the
    neighbour to the left, or sat under the one above, wins. That is what
    keeps a run of panels reading as one thing instead of a row of
    fragments.
    """
    h, w = len(grid), len(grid[0])
    out = [[EMPTY] * w for _ in range(h)]

    def solid(x, y):
        return not (0 <= x < w and 0 <= y < h) or grid[y][x] == SOLID

    def context(x, y):
        bits = 0
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                bits = (bits << 1) | (1 if solid(x + dx, y + dy) else 0)
        return bits

    misses = 0
    for y in range(h):
        for x in range(w):
            candidates = model["context"].get(context(x, y)) if model else None
            if candidates and theme:
                inside = [c for c in candidates if in_theme(c, theme["fg"])]
                candidates = inside or None

            if not candidates:
                # An arrangement the originals never built. Falls back to
                # the old fixed sets, which at least respect solidity.
                misses += 1
                if not solid(x, y):
                    out[y][x] = EMPTY
                elif not solid(x, y - 1):
                    out[y][x] = pick(FLOOR, x, y)
                elif not solid(x, y + 1):
                    out[y][x] = pick(CEILING, x, y)
                else:
                    out[y][x] = pick(FILL, x, y)
                continue

            left = out[y][x - 1] if x > 0 else None
            above = out[y - 1][x] if y > 0 else None

            scored = []
            for rank, cand in enumerate(candidates):
                # How usual the tile is here, then how well it follows what
                # is already beside and above it.
                score = (len(candidates) - rank) * 2
                if left is not None:
                    after = model["right_of"].get(left, [])
                    if cand in after:
                        score += 12 - after.index(cand)
                if above is not None:
                    under = model["below"].get(above, [])
                    if cand in under:
                        score += 12 - under.index(cand)
                scored.append((score, cand))

            # Among the tiles that fit equally well, the position decides.
            # Taking the single best every time gave a wall of one brick
            # repeated, which is coherent and lifeless; the originals vary
            # because the artists varied. The margin is smaller than an
            # adjacency bonus, so fitting the neighbours still wins.
            best = max(score for score, _ in scored)
            good = [c for score, c in scored if score >= best - TIE_MARGIN]
            out[y][x] = pick(good, x, y, 5)

    if misses:
        print(f"{misses} cells had a shape the originals never built")
    return out


def background(bw, bh, model, theme):
    """Sparse machinery over black, stamped in whole blocks.

    Single tiles scattered about was the first version, and it put
    fragments of machines in the dark with nothing to explain them. The
    blocks come from the shipped levels and go down intact.
    """
    out = [[EMPTY] * bw for _ in range(bh)]
    patches = (model or {}).get("bg_patch") or []
    if theme:
        patches = [p for p in patches
                   if all(t == EMPTY or in_theme(t, theme["bg"]) for t in p)]
    if not patches:
        for y in range(bh):
            for x in range(bw):
                if pick(range(100), x, y, 4) < BACK_DENSITY:
                    out[y][x] = pick(BACK, x, y, 3)
        return out

    # A block every so often, on a loose grid so they do not line up. The
    # density is what the shipped levels have: level02's background is 91%
    # empty, and filling it makes the background compete with the
    # foreground until a player cannot tell what is solid.
    step = 5
    for gy in range(0, bh, step):
        for gx in range(0, bw, step):
            if pick(range(100), gx, gy, 4) >= BACK_DENSITY * step * step // 3:
                continue
            patch = pick(patches, gx, gy, 3)
            ox = gx + pick(range(step - 2), gx, gy, 6)
            oy = gy + pick(range(step - 2), gx, gy, 7)
            for dy in range(3):
                for dx in range(3):
                    x, y = ox + dx, oy + dy
                    if 0 <= x < bw and 0 <= y < bh:
                        out[y][x] = patch[dy * 3 + dx]
    return out


def pack_map(tiles):
    h, w = len(tiles), len(tiles[0])
    body = b"".join(struct.pack("<%dH" % w, *row) for row in tiles)
    return struct.pack("<II", w, h) + body


def build(args):
    head, grid = read_map(args.mapfile)
    errors, _, _ = check(grid)
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

    model = load_model()
    theme_name = head.get("theme", "techno")
    theme = THEMES.get(theme_name)
    if theme_name and not theme:
        sys.exit(f"unknown theme '{theme_name}'; known: "
                 + ", ".join(sorted(THEMES)))
    print(f"theme {theme_name}")

    tiles = autotile(grid, model, theme)
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
    write_entry(str(out), ids["bgmap"], 19, "bgmap",
                pack_map(background(bw, bh, model, theme)))
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

    # Which mark each object type answers to, from ROLES.
    want = {}
    for mark, wanted in ROLES.items():
        for name in wanted:
            want.setdefault(name, mark)

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
          (", ".join(f"{k}x{v}" for k, v in sorted(used.items())) or "none"))
    for mark, spots in sorted(marks.items()):
        if used[mark] < len(spots):
            print(f"  '{mark}': {len(spots)} marks, {used[mark]} filled; "
                  f"the template has no more")


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
    errors, seen, _ = check(grid)
    h, w = len(grid), len(grid[0])
    solid = sum(row.count(SOLID) for row in grid)
    print(f"{args.mapfile}: {w}x{h} tiles, {w * tile_px(0)}x{h * tile_px(1)} pixels")
    print(f"  solid {solid} of {w * h} ({100 * solid // (w * h)}%), "
          f"reachable open cells {len(seen)}")

    for e in errors:
        print(f"error: {e}", file=sys.stderr)
    return 1 if errors else 0


def clusters(cells, apart=6, most=8):
    """A handful of representative spots, so the report names places and
    not a hundred neighbouring cells."""
    out = []
    for x, y in sorted(cells):
        if all(abs(x - ox) > apart or abs(y - oy) > apart for ox, oy in out):
            out.append((x, y))
            if len(out) >= most:
                break
    return out


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
