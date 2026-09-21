/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Video presentation options: how the finished framebuffer reaches the
 *  window. Phase 2, task 2.2.
 *
 *  None of this touches what the game draws, only how the finished frame is
 *  scaled and filtered on the way out.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_RENDER_OPTIONS_H_
#define ABUSE_RENDER_OPTIONS_H_

#include <stdint.h>

namespace abuse::render {

enum class ScaleMode
{
    Integer,    // whole multiples only, black bars around
    Fit,        // largest fit that keeps the aspect ratio
    Stretch     // fill the window, aspect ratio ignored
};

// How the finished frame reaches the window.
//
// Classic is SDL_Renderer, which is what every snapshot in the suite was
// taken through and what the tests still use. Gpu is SDL_GPU, decided on
// 2026-09-20: the palette conversion moves off the CPU, which is the
// ceiling the visual phase keeps running into. Opt-in, and it falls back
// to Classic when the device cannot be had.
enum class Backend
{
    Classic,
    Gpu
};

bool parse_backend(char const *name, Backend &out);
char const *backend_name(Backend b);

enum class Filter
{
    Nearest,    // hard pixel edges
    Linear,     // blurred
    PixelArt    // nearest with smoothed edges at non-integer factors
};

// The shape of the picture. The game draws into a buffer 200 pixels tall and
// the presentation stretches it to 240, so the width that gives a ratio is
// 240 times it: 320 for 4:3, 427 for 16:9.
//
// What wakes up in a level does not follow this; see view::classic_xoff.
// A wider picture shows more of the room and fights the same fight, which
// is what makes it safe to offer at all.
enum class Aspect
{
    Classic,    // 4:3, the shape the levels were drawn for
    Wide16x10,
    Wide16x9,
    Ultra21x9
};

int aspect_width(Aspect a);
char const *aspect_name(Aspect a);
bool parse_aspect(char const *name, Aspect &out);

struct Options
{
    Backend backend = Backend::Classic;
    ScaleMode scale = ScaleMode::Fit;
    Filter filter = Filter::PixelArt;
    bool vsync = true;
    int fps_limit = 0;              // 0 = no limit beyond vsync
    uint8_t letterbox[3] = {0, 0, 0};

    // The shape of the picture, which only takes effect when the game starts:
    // the buffer, the views and the light table are all sized from it.
    Aspect aspect = Aspect::Classic;

    // Dark lines between the game's pixel rows, the way a CRT left one. Only
    // where there is room for them: at least two window rows per game row.
    bool scanlines = false;

    // Accessibility: one switch over every effect that moves the picture
    // by itself, for people who get motion sick from them. It vetoes
    // rather than replaces, so the individual settings survive being
    // turned off collectively and come back when it is turned off.
    bool reduce_motion = false;

    // Draw positions blended between the last two logical ticks. The world
    // still advances 15 times a second; this is only about what is shown in
    // between. Phase 6, block 6.1.
    //
    // Off, after being watched by a person for the first time on 2026-09-19:
    // it makes the character skate. The art is animated at the tick rate, so
    // gliding the body between two positions while the legs keep their
    // fifteen frames a second reads as running too fast rather than as
    // running smoothly. See the note in docs/plan/fase-06-visual.md.
    bool interpolate = false;
};

// Whether an effect that moves the picture on its own may run at all. The
// per-effect switch still decides on top of this; this is only the veto, so
// that an effect added later is covered without touching the setting.
bool motion_allowed();

// Parsing, kept away from SDL so it can be unit tested. Both return false and
// leave the output untouched when the name is unknown, so a typo in abuserc
// falls back to the default rather than to something arbitrary.
bool parse_scale_mode(char const *name, ScaleMode &out);
bool parse_filter(char const *name, Filter &out);

// Names as written in abuserc. Round trip with the parsers above.
char const *scale_mode_name(ScaleMode m);
char const *filter_name(Filter f);

// "on"/"off", and the spellings a person actually types. False and untouched
// for anything else, like the parsers above.
bool parse_switch(char const *text, bool &out);

// "rrggbb", with or without a leading '#'.
bool parse_letterbox(char const *text, uint8_t out[3]);

// Named combinations, so the common cases need one setting instead of two.
// "classic" is what the Original mode always uses.
enum class Preset
{
    Classic,    // fit + pixelart, the upstream look, nothing added
    Sharp,      // integer + nearest, hard pixels and no partial scaling
    Enhanced,   // the GPU path with the lighting and the effects on
    Crt         // Enhanced plus scanlines
};

bool parse_preset(char const *name, Preset &out);
char const *preset_name(Preset p);

// Overwrites the look: scale, filter, backend, scanlines and the lighting.
// Leaves vsync, the fps limit, the letterbox and the aspect alone, because
// those describe the display and the window rather than the picture, and a
// player who has set them did not ask for a preset to undo it.
//
// Classic turns the additions off rather than leaving them, which is what
// makes it usable as the Original mode's reset.
void apply_preset(Preset p, Options &opt);

// Whether the preset wants lighting in RGB. Separate because the lighting
// lives in its own module and apply_preset only touches this one.
bool preset_wants_rgb_light(Preset p);

Options &options();

}

#endif
