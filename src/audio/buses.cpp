/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See buses.h.
 *
 *  This software was released into the Public Domain.
 */

#include "buses.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"

namespace abuse::audio {

namespace {

float g_gain[kBusCount] = { 1.0f, 1.0f, 1.0f };
float g_master = 1.0f;

float clamp_gain(float g)
{
    if (g < 0.0f)
        return 0.0f;
    if (g > 2.0f)
        return 2.0f;
    return g;
}

}

void set_gain(Bus b, float g)
{
    if ((int)b >= 0 && (int)b < kBusCount)
        g_gain[(int)b] = clamp_gain(g);
}

float gain(Bus b)
{
    if ((int)b >= 0 && (int)b < kBusCount)
        return g_gain[(int)b];
    return 1.0f;
}

void set_master(float g)
{
    g_master = clamp_gain(g);
}

float master()
{
    return g_master;
}

float voice_gain(Bus b, int volume)
{
    if (volume <= 0)
        return 0.0f;
    if (volume > 255)
        volume = 255;

    float v = (float)volume / 255.0f * gain(b) * master();
    if (v > 1.0f)
        return 1.0f;
    return v;
}

char const *bus_name(Bus b)
{
    switch (b)
    {
    case Bus::Sfx:   return "sfx";
    case Bus::Music: return "music";
    case Bus::Ui:    return "ui";
    case Bus::Count: break;
    }
    return "sfx";
}

bool parse_bus(char const *name, Bus &out)
{
    if (!name)
        return false;
    for (int i = 0; i < kBusCount; i++)
    {
        Bus b = (Bus)i;
        if (strcasecmp(name, bus_name(b)) == 0)
        {
            out = b;
            return true;
        }
    }
    return false;
}

bool parse_percent(char const *text, float &g)
{
    if (!text || !*text)
        return false;

    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (!end || *end || value < 0 || value > 200)
        return false;

    g = (float)value / 100.0f;
    return true;
}

int percent(float g)
{
    int p = (int)(g * 100.0f + 0.5f);
    if (p < 0)
        return 0;
    if (p > 200)
        return 200;
    return p;
}

}
