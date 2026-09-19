/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See rumble.h.
 *
 *  This software was released into the Public Domain.
 */

#include "rumble.h"

namespace abuse::input {

namespace {

RumbleSettings g_settings;

uint16_t scaled(int base, int strength)
{
    long v = (long)base * strength / 100;
    if (v < 0)
        return 0;
    if (v > 0xffff)
        return 0xffff;
    return (uint16_t)v;
}

}

RumbleCommand rumble_for(RumbleEvent e, RumbleSettings const &s)
{
    RumbleCommand c;
    if (s.strength <= 0)
        return c;               // silent, and the caller need not special case it

    switch (e)
    {
    case RumbleEvent::Shot:
        // Short and light: it happens many times a second and must not blur
        // into one continuous buzz.
        c.low = scaled(0x1000, s.strength);
        c.high = scaled(0x3000, s.strength);
        c.duration_ms = 60;
        break;
    case RumbleEvent::Hurt:
        c.low = scaled(0x8000, s.strength);
        c.high = scaled(0x4000, s.strength);
        c.duration_ms = 180;
        break;
    case RumbleEvent::Explosion:
        c.low = scaled(0xc000, s.strength);
        c.high = scaled(0x6000, s.strength);
        c.duration_ms = 300;
        break;
    }
    return c;
}

bool parse_rumble_strength(char const *text, int &out)
{
    if (!text || !*text)
        return false;

    int value = 0;
    for (char const *c = text; *c; c++)
    {
        if (*c < '0' || *c > '9')
            return false;
        value = value * 10 + (*c - '0');
        if (value > 100)
            return false;
    }

    out = value;
    return true;
}

RumbleSettings &rumble_settings()
{
    return g_settings;
}

}
