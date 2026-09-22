/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See dynlight.h.
 *
 *  This software was released into the Public Domain.
 */

#include "dynlight.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace abuse::render {

namespace {

std::vector<Emitter> g_emitters;
bool g_enabled = true;

bool blank(char c) { return c == ' ' || c == '\t' || c == '\r'; }

int clamp_byte(long v) { return (int)(v < 0 ? 0 : v > 255 ? 255 : v); }

// A hash, not a random number. The waver has to be the same on every
// machine replaying the same tick, and it must not touch the game's RNG,
// whose cursor is part of the state hash. Splitmix64's finaliser, which is
// cheap and mixes low bits well.
uint32_t mix(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return (uint32_t)((x ^ (x >> 31)) & 0xffffffffULL);
}

}

int parse_emitters(char const *text, std::vector<Emitter> &out)
{
    out.clear();
    if (!text)
        return 0;

    int bad = 0;

    char const *p = text;
    while (*p)
    {
        char const *eol = strchr(p, '\n');
        std::string line(p, eol ? (size_t)(eol - p) : strlen(p));
        p = eol ? eol + 1 : p + line.size();

        size_t hash = line.find('#');
        if (hash != std::string::npos)
            line.resize(hash);

        size_t at = 0;
        while (at < line.size() && blank(line[at]))
            at++;
        if (at >= line.size())
            continue;

        Emitter e;
        size_t start = at;
        while (at < line.size() && !blank(line[at]))
            at++;
        e.name = line.substr(start, at - start);

        // strtol over sscanf: the name is already taken and the two numbers
        // want the same "nothing there" answer as a name with no numbers.
        char *end = nullptr;
        e.radius = (int)strtol(line.c_str() + at, &end, 10);
        if (end == line.c_str() + at)
        {
            bad++;
            continue;
        }
        char *end2 = nullptr;
        e.strength = (int)strtol(end, &end2, 10);
        if (end2 == end)
        {
            bad++;
            continue;
        }

        // The colour and the waver are optional, and all four together or
        // none: a line with one or two colour channels is a typo, not a
        // light, and taking the two it has would put out a colour nobody
        // asked for.
        char *end3 = nullptr;
        long const red = strtol(end2, &end3, 10);
        if (end3 != end2)
        {
            char *end4 = nullptr;
            long const green = strtol(end3, &end4, 10);
            char *end5 = nullptr;
            long const blue = strtol(end4, &end5, 10);
            if (end4 == end3 || end5 == end4)
            {
                bad++;
                continue;
            }

            e.r = clamp_byte(red);
            e.g = clamp_byte(green);
            e.b = clamp_byte(blue);

            char *end6 = nullptr;
            long const waver = strtol(end5, &end6, 10);
            if (end6 != end5)
            {
                e.flicker = (int)(waver < 0 ? 0 : waver > 100 ? 100 : waver);
            }
        }

        if (e.radius <= 0 || e.strength <= 0)
        {
            bad++;
            continue;
        }
        if (e.strength > kFullLight)
            e.strength = kFullLight;

        out.push_back(e);
    }

    return bad;
}

int load_emitters(char const *text)
{
    return parse_emitters(text, g_emitters);
}

std::vector<Emitter> const &emitters() { return g_emitters; }

void emitter_strength(Emitter const &e, int tick, int seed, int &sr, int &sg,
                      int &sb)
{
    int strength = e.strength;

    if (e.flicker > 0)
    {
        // Between (100 - flicker)% and 100% of the strength, never above
        // it: a light that brightens past what the table says would wash
        // out the frame on the tick it happened to peak.
        uint32_t const r = mix(((uint64_t)(uint32_t)tick << 32)
                               ^ (uint32_t)seed) % 1000u;
        int const down = (int)((uint64_t)strength * e.flicker * r / 100000u);
        strength -= down;
    }

    sr = strength * e.r / 255;
    sg = strength * e.g / 255;
    sb = strength * e.b / 255;
}

void brighten(LightMap &map, int cx, int cy, int radius, int sr, int sg,
              int sb)
{
    int const strength = sr > sg ? (sr > sb ? sr : sb) : (sg > sb ? sg : sb);
    if (map.empty() || radius <= 0 || strength <= 0)
        return;

    int const x1 = cx - radius < 0 ? 0 : cx - radius;
    int const y1 = cy - radius < 0 ? 0 : cy - radius;
    int const x2 = cx + radius >= map.width() ? map.width() - 1 : cx + radius;
    int const y2 = cy + radius >= map.height() ? map.height() - 1 : cy + radius;

    float const reach = (float)radius;

    for (int y = y1; y <= y2; y++)
    {
        int const dy = y - cy;
        for (int x = x1; x <= x2; x++)
        {
            int const dx = x - cx;
            float const d = sqrtf((float)(dx * dx + dy * dy));
            if (d >= reach)
                continue;

            float const fade = 1.0f - d / reach;
            map.add(x, y, (int)(fade * (float)sr), (int)(fade * (float)sg),
                    (int)(fade * (float)sb));
        }
    }
}

bool dynlight_enabled() { return g_enabled; }
void set_dynlight_enabled(bool on) { g_enabled = on; }

}
