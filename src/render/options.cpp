/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See options.h.
 *
 *  This software was released into the Public Domain.
 */

#include "options.h"

#include <string.h>
#include "compat.h"

namespace abuse::render {

namespace {

Options g_options;

bool equals(char const *a, char const *b)
{
    return a && b && strcasecmp(a, b) == 0;
}

int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

}

bool parse_scale_mode(char const *name, ScaleMode &out)
{
    if (equals(name, "integer"))
        out = ScaleMode::Integer;
    else if (equals(name, "fit"))
        out = ScaleMode::Fit;
    else if (equals(name, "stretch"))
        out = ScaleMode::Stretch;
    else
        return false;
    return true;
}

bool parse_filter(char const *name, Filter &out)
{
    if (equals(name, "nearest"))
        out = Filter::Nearest;
    else if (equals(name, "linear"))
        out = Filter::Linear;
    else if (equals(name, "pixelart"))
        out = Filter::PixelArt;
    else if (equals(name, "scale2x"))
        out = Filter::Scale2x;
    else
        return false;
    return true;
}

char const *scale_mode_name(ScaleMode m)
{
    switch (m)
    {
    case ScaleMode::Integer: return "integer";
    case ScaleMode::Stretch: return "stretch";
    case ScaleMode::Fit:     break;
    }
    return "fit";
}

char const *filter_name(Filter f)
{
    switch (f)
    {
    case Filter::Nearest: return "nearest";
    case Filter::Linear: return "linear";
    case Filter::Scale2x: return "scale2x";
    default: return "pixelart";
    }
}

namespace {

struct AspectEntry
{
    Aspect aspect;
    char const *name;
    int width;
};

// 240 times the ratio, rounded: the buffer is 200 tall and is presented as
// 240, which is where the aspect correction of the original lives.
AspectEntry const kAspects[] = {
    { Aspect::Classic,   "4:3",   320 },
    { Aspect::Wide16x10, "16:10", 384 },
    { Aspect::Wide16x9,  "16:9",  427 },
    { Aspect::Ultra21x9, "21:9",  560 },
};

}

int aspect_width(Aspect a)
{
    for (AspectEntry const &e : kAspects)
        if (e.aspect == a)
            return e.width;
    return 320;
}

char const *aspect_name(Aspect a)
{
    for (AspectEntry const &e : kAspects)
        if (e.aspect == a)
            return e.name;
    return "4:3";
}

bool parse_aspect(char const *name, Aspect &out)
{
    if (!name)
        return false;
    for (AspectEntry const &e : kAspects)
        if (equals(name, e.name))
        {
            out = e.aspect;
            return true;
        }
    return false;
}

bool parse_switch(char const *text, bool &out)
{
    if (!text)
        return false;
    if (equals(text, "on") || equals(text, "true") || equals(text, "yes")
        || equals(text, "1"))
    {
        out = true;
        return true;
    }
    if (equals(text, "off") || equals(text, "false") || equals(text, "no")
        || equals(text, "0"))
    {
        out = false;
        return true;
    }
    return false;
}

bool parse_letterbox(char const *text, uint8_t out[3])
{
    if (!text)
        return false;
    if (*text == '#')
        text++;
    if (strlen(text) != 6)
        return false;

    uint8_t parsed[3];
    for (int i = 0; i < 3; i++)
    {
        int hi = hex_digit(text[i * 2]);
        int lo = hex_digit(text[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        parsed[i] = (uint8_t)(hi * 16 + lo);
    }

    memcpy(out, parsed, sizeof(parsed));
    return true;
}

namespace {

char const *preset_label(Preset p)
{
    switch (p)
    {
    case Preset::Sharp: return "sharp";
    case Preset::Enhanced: return "enhanced";
    case Preset::Crt: return "crt";
    default: return "classic";
    }
}

}

bool parse_preset(char const *name, Preset &out)
{
    if (equals(name, "classic"))
        out = Preset::Classic;
    else if (equals(name, "sharp"))
        out = Preset::Sharp;
    else if (equals(name, "enhanced"))
        out = Preset::Enhanced;
    else if (equals(name, "crt"))
        out = Preset::Crt;
    else
        return false;
    return true;
}

char const *preset_name(Preset p)
{
    return preset_label(p);
}

void apply_preset(Preset p, Options &opt)
{
    // Everything off first, so a preset is a statement of what the picture
    // is and not an accumulation of whatever was on before it.
    opt.scale = ScaleMode::Fit;
    opt.filter = Filter::PixelArt;
    opt.backend = Backend::Classic;
    opt.scanlines = false;
    opt.bloom = 0.0f;

    switch (p)
    {
    case Preset::Sharp:
        opt.scale = ScaleMode::Integer;
        opt.filter = Filter::Nearest;
        break;

    case Preset::Crt:
        opt.scanlines = true;
        opt.backend = Backend::Gpu;
        opt.bloom = 0.6f;
        break;

    case Preset::Enhanced:
        opt.backend = Backend::Gpu;
        opt.bloom = 0.6f;
        break;

    case Preset::Classic:
        break;
    }
}

// The lighting lives in its own module, so a preset that wants it has to
// say so separately. True for the presets that mean "use what the machine
// can do", false for the ones that mean "as it was".
bool preset_wants_rgb_light(Preset p)
{
    return p == Preset::Enhanced || p == Preset::Crt;
}

bool parse_backend(char const *name, Backend &out)
{
    if (!name)
        return false;
    if (strcasecmp(name, "classic") == 0 || strcasecmp(name, "sdl") == 0)
    {
        out = Backend::Classic;
        return true;
    }
    if (strcasecmp(name, "gpu") == 0 || strcasecmp(name, "sdlgpu") == 0)
    {
        out = Backend::Gpu;
        return true;
    }
    return false;
}

char const *backend_name(Backend b)
{
    return b == Backend::Gpu ? "gpu" : "classic";
}

Options &options()
{
    return g_options;
}

bool motion_allowed()
{
    return !g_options.reduce_motion;
}

}
