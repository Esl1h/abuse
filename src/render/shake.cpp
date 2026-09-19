/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See shake.h.
 *
 *  This software was released into the Public Domain.
 */

#include "shake.h"

#include <stdint.h>

namespace abuse::render {

namespace {

// Eight game pixels is already a lot on a 320 by 200 screen: the ceiling is
// there so a burst of hits cannot walk the camera off what is drawn.
float const kMax = 8.0f;

// Per tick. At 15 Hz this dies out in about a fifth of a second, which is
// long enough to feel and short enough not to be a nuisance while fighting.
float const kDecay = 0.55f;

// Below this there is nothing left to see, and letting it crawl to zero
// would keep the camera off centre by a pixel for no reason.
float const kFloor = 0.5f;

float g_amount = 0.0f;
bool g_enabled = true;

// Its own sequence. Touching the game's RNG here would change every roll the
// simulation makes afterwards, and the replays would stop matching.
uint32_t g_seed = 0x9e3779b9u;

int next_offset(float amount)
{
    // xorshift, which is plenty for a camera wobble.
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;

    int range = (int)amount;
    if (range < 1)
        return 0;

    return (int)(g_seed % (uint32_t)(range * 2 + 1)) - range;
}

}

void shake(float amount)
{
    if (!g_enabled || amount <= 0.0f)
        return;

    g_amount += amount;
    if (g_amount > kMax)
        g_amount = kMax;
}

void shake_tick()
{
    if (g_amount <= 0.0f)
        return;

    g_amount *= kDecay;
    if (g_amount < kFloor)
        g_amount = 0.0f;
}

void shake_offset(int &dx, int &dy)
{
    dx = 0;
    dy = 0;
    if (!g_enabled || g_amount <= 0.0f)
        return;

    dx = next_offset(g_amount);
    dy = next_offset(g_amount);
}

float shake_amount()
{
    return g_amount;
}

void reset_shake()
{
    g_amount = 0.0f;
}

bool shake_enabled()
{
    return g_enabled;
}

void set_shake_enabled(bool on)
{
    g_enabled = on;
    if (!on)
        g_amount = 0.0f;
}

}
