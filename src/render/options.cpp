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
    case Filter::Nearest:  return "nearest";
    case Filter::Linear:   return "linear";
    case Filter::PixelArt: break;
    }
    return "pixelart";
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

bool parse_preset(char const *name, Preset &out)
{
    if (equals(name, "classic"))
        out = Preset::Classic;
    else if (equals(name, "sharp"))
        out = Preset::Sharp;
    else
        return false;
    return true;
}

char const *preset_name(Preset p)
{
    return p == Preset::Sharp ? "sharp" : "classic";
}

void apply_preset(Preset p, Options &opt)
{
    switch (p)
    {
    case Preset::Sharp:
        opt.scale = ScaleMode::Integer;
        opt.filter = Filter::Nearest;
        break;
    case Preset::Classic:
        opt.scale = ScaleMode::Fit;
        opt.filter = Filter::PixelArt;
        break;
    }
}

Options &options()
{
    return g_options;
}

}
