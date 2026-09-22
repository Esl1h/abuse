/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Objects that light the room around them. Phase 6, block 6.4.
 *
 *  The 1995 game already knows how to do this: an explosion drops an
 *  EXP_LIGHT object, which adds a real light source for three ticks. The
 *  muzzle flash was written the same way and then commented out, in
 *  weapons.lsp, with the note "add cool light if not too slow". On a 1995
 *  machine a light source per shot meant rebuilding the patch list every
 *  tick, and it was too slow.
 *
 *  It is not too slow now, but the Lisp is off limits (rule 6 of AGENTS.md),
 *  so this does the same thing on the other side of the frame: after the
 *  lighting pass has filled the light map, every active object whose type is
 *  in the table brightens the map around itself. Nothing in the simulation is
 *  touched, no light source is created or destroyed, and a replay hashes the
 *  same with this on or off.
 *
 *  Which types glow, how far and how much is a table in data, `dynlight.txt`,
 *  and not a list in here: whether a plasma shot lights a corridor is a
 *  question for whoever tunes the game, and the Remastered data may want to
 *  answer it differently from the Original.
 *
 *  Only on the RGB path. The 1995 path cannot brighten anything: it lights a
 *  frame by replacing palette indices through a table that only darkens.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_RENDER_DYNLIGHT_H_
#define ABUSE_RENDER_DYNLIGHT_H_

#include <string>
#include <vector>

#include "lightmap.h"

namespace abuse::render {

// One light-emitting object type, as the table names it.
struct Emitter
{
    std::string name;   // the object type, as `def_char` spells it
    int radius = 0;     // reach in world pixels
    int strength = 0;   // light levels added at the centre, 1 to 63
};

// Reads the table. One entry per line, `NAME radius strength`, with `#` to
// end of line as a comment. A line that does not parse is dropped and
// counted, not fatal: a typo in a data file should cost one light, not the
// level.
//
// Returns the number of lines that were meant to be entries and were not.
int parse_emitters(char const *text, std::vector<Emitter> &out);

// Parses the table into the one the drawer reads, replacing whatever was
// there. The text comes from the caller rather than from a file so that
// this module stays free of the data directory and of imlib, and can be
// unit tested with a string.
//
// Returns what parse_emitters returns.
int load_emitters(char const *text);

std::vector<Emitter> const &emitters();

// Brightens the map around a point in screen coordinates, falling off
// linearly to nothing at `radius`. Adds light: a pixel already at full
// brightness stays there, and the darkest corner of a level is what this is
// for.
void brighten(LightMap &map, int cx, int cy, int radius, int strength);

// The setting. Off skips the walk over the active list entirely.
bool dynlight_enabled();
void set_dynlight_enabled(bool on);

}

#endif
