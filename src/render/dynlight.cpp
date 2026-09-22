/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See dynlight.h.
 *
 *  This software was released into the Public Domain.
 */

#include "dynlight.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace abuse::render {

namespace {

std::vector<Emitter> g_emitters;
bool g_enabled = true;

bool blank(char c) { return c == ' ' || c == '\t' || c == '\r'; }

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

void brighten(LightMap &map, int cx, int cy, int radius, int strength)
{
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

            int level = map.at(x, y)
                        + (int)((1.0f - d / reach) * (float)strength);
            map.fill(x, y, 1, level > kFullLight ? kFullLight : level);
        }
    }
}

bool dynlight_enabled() { return g_enabled; }
void set_dynlight_enabled(bool on) { g_enabled = on; }

}
