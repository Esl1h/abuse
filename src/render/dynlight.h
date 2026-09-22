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

    // The colour, 0 to 255 a channel, as a share of the strength. White is
    // the three at 255 and is the same light as before colour existed.
    //
    // A light never adds colour to the picture: it takes less off the
    // channels it is made of, and that is what reads as red. On a grey wall
    // a red flash leaves the red where it is and lets the other two stay
    // dark, which is the same thing a real one does.
    int r = 255, g = 255, b = 255;

    // How much the light wavers, 0 to 100, as a percentage of the strength.
    // 0 is a steady light. It changes once a logical tick, not once a
    // frame: at 165 frames a second a per-frame waver is a strobe.
    int flicker = 0;
};

// Reads the table. One entry per line,
//
//     NAME radius strength [r g b [flicker]]
//
// with `#` to end of line as a comment. The colour and the flicker are
// optional, and leaving them out is a steady white light. A line that does
// not parse is dropped and counted, not fatal: a typo in a data file should
// cost one light, not the level.
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
//
// The strength is per channel, so the caller has already folded the colour
// and the flicker into it.
void brighten(LightMap &map, int cx, int cy, int radius, int sr, int sg,
              int sb);

// The strength this emitter has on a given tick, per channel, with its
// colour and its waver applied. `seed` separates two objects of the same
// type on the same tick; the object's own position does.
//
// Deterministic and free of the game's RNG: the waver is a hash, not a
// draw, so nothing the simulation counts on moves, and a replay hashes the
// same with the lights on or off.
void emitter_strength(Emitter const &e, int tick, int seed, int &sr, int &sg,
                      int &sb);

// The setting. Off skips the walk over the active list entirely.
bool dynlight_enabled();
void set_dynlight_enabled(bool on);

}

#endif
