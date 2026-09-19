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

enum class Filter
{
    Nearest,    // hard pixel edges
    Linear,     // blurred
    PixelArt    // nearest with smoothed edges at non-integer factors
};

struct Options
{
    ScaleMode scale = ScaleMode::Fit;
    Filter filter = Filter::PixelArt;
    bool vsync = true;
    int fps_limit = 0;              // 0 = no limit beyond vsync
    uint8_t letterbox[3] = {0, 0, 0};

    // Dark lines between the game's pixel rows, the way a CRT left one. Only
    // where there is room for them: at least two window rows per game row.
    bool scanlines = false;

    // Draw positions blended between the last two logical ticks. The world
    // still advances 15 times a second; this is only about what is shown in
    // between. Phase 6, block 6.1.
    bool interpolate = true;
};

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
    Classic,    // fit + pixelart, the upstream look
    Sharp       // integer + nearest, hard pixels and no partial scaling
};

bool parse_preset(char const *name, Preset &out);
char const *preset_name(Preset p);

// Overwrites scale and filter; leaves vsync, fps limit and letterbox alone,
// since those are about the display and not about the look.
void apply_preset(Preset p, Options &opt);

Options &options();

}

#endif
