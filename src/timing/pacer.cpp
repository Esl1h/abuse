/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See pacer.h.
 *
 *  This software was released into the Public Domain.
 */

#include "pacer.h"

namespace abuse::timing {

Pacer::Pacer(double tick_ms)
    : m_tick_ms(tick_ms > 0.0 ? tick_ms : kTickMs)
{
}

int Pacer::advance(double elapsed_ms)
{
    m_dropped = false;

    // A clock that went backwards, or a bogus reading, must not owe ticks.
    if (elapsed_ms > 0.0)
        m_accumulator += elapsed_ms;

    // Compared with a slack of a thousandth of a tick. Feeding exactly one
    // tick of elapsed time can land a hair under it once the value has been
    // through a couple of multiplications, and a pacer that answers zero to
    // exactly one tick would run the game at half speed on a display whose
    // refresh divides evenly into the tick rate.
    double const slack = m_tick_ms * 1e-3;

    int ticks = 0;
    while (m_accumulator + slack >= m_tick_ms && ticks < kMaxCatchUp)
    {
        m_accumulator -= m_tick_ms;
        if (m_accumulator < 0.0)
            m_accumulator = 0.0;
        ticks++;
    }

    if (m_accumulator + slack >= m_tick_ms)
    {
        // Still owing after the cap: forget the rest instead of chasing it.
        m_dropped = true;
        m_accumulator = 0.0;
    }

    return ticks;
}

float Pacer::alpha() const
{
    return (float)(m_accumulator / m_tick_ms);
}

void Pacer::reset()
{
    m_accumulator = 0.0;
    m_dropped = false;
}

}
