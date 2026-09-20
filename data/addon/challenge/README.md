# Challenge pack

Standalone maps for the Remastered mode: one sitting each, a timer, a best
time, no campaign to carry on.

**This is a skeleton.** There is no map in it yet. It is here so that the
layout, the tile numbering and the licence entry are settled before any
content arrives, and so that nothing later has to move.

```sh
abuse-vrenna -a challenge
```

## What goes where

```text
addon/challenge/
├── challenge.lsp     # the whole interface to the add-on system
├── challenge.spe     # tiles of our own, when there are any
└── maps/             # one .lvl per map
```

## Rules this pack lives by

- **Nothing outside this directory changes.** Not the campaign, not the
  engine, not `data/lisp/`. An add-on is a separate world by construction.
- **Foreground tiles are numbered from 1200, background tiles from 350.** The
  base game uses everything below that. This is the original add-on
  convention and it still holds.
- **Every asset is registered** in `data/MANIFEST.toml`, and anything
  AI-generated records the tool, the model and the date. That is rule 3 of
  `AGENTS.md` and it is not optional.
- **Nothing here exists in the Original mode.**

## On the maps being homages

The maps are built to evoke films and games people know. The line the pack
does not cross is the difference between a genre and a design.

A derelict cargo hauler with a distress signal, a polar outpost nobody can
leave, a buried desert structure with something under the sand: these are
situations, and situations belong to everyone.

A recognisable creature design, a corporate logo, a named character, a
specific ship's floor plan: these are somebody's work, and copying them
infringes whatever the map is called. Leaving the name off the title is a
trademark measure and does nothing about copyright.

What makes an homage land is the pacing anyway, not a traced monster: the
long silent corridor before the ambush, the open arena that floods, the
corridor that closes behind you. Pacing is layout, and layout is ours.
