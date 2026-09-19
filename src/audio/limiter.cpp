/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See limiter.h.
 *
 *  This software was released into the Public Domain.
 */

#include "limiter.h"

#include <math.h>

namespace abuse::audio {

namespace {

// Time constants. Fast enough to catch the front of an explosion, slow
// enough on the way back that a run of shots does not pump.
float const kAttackMs = 2.0f;
float const kReleaseMs = 150.0f;

// The per-sample step that covers the distance in `ms` at `rate`.
float coefficient(float ms, int rate)
{
    if (rate <= 0 || ms <= 0.0f)
        return 1.0f;

    float samples = ms * (float)rate / 1000.0f;
    if (samples < 1.0f)
        return 1.0f;
    return 1.0f / samples;
}

Limiter g_limiter;

}

void Limiter::configure(int rate, int channels)
{
    m_channels = channels > 0 ? channels : 2;
    m_attack = coefficient(kAttackMs, rate);
    m_release = coefficient(kReleaseMs, rate);
    reset();
}

void Limiter::set_ceiling(float c)
{
    if (c < 0.01f)
        c = 0.01f;
    if (c > 1.0f)
        c = 1.0f;
    m_ceiling = c;
}

void Limiter::set_enabled(bool on)
{
    m_enabled = on;
    if (!on)
        reset();
}

void Limiter::reset()
{
    m_gain = 1.0f;
}

void Limiter::process(float *pcm, int samples)
{
    if (!m_enabled || !pcm || samples <= 0)
        return;

    // A frame is one sample per channel. The gain moves per frame and is the
    // same across channels, because moving the channels independently would
    // shift the stereo image every time something loud happened on one side.
    int const channels = m_channels > 0 ? m_channels : 1;

    for (int i = 0; i + channels <= samples; i += channels)
    {
        float peak = 0.0f;
        for (int c = 0; c < channels; c++)
        {
            float v = fabsf(pcm[i + c]);
            if (v > peak)
                peak = v;
        }

        // What this frame would need to sit under the ceiling.
        float wanted = 1.0f;
        if (peak * m_gain > m_ceiling && peak > 0.0f)
            wanted = m_ceiling / peak;

        // Down fast, up slowly. Both as a step toward the target rather than
        // a jump to it, which is what keeps the movement from being audible
        // as its own sound.
        if (wanted < m_gain)
        {
            m_gain += (wanted - m_gain) * m_attack;
            if (m_gain < wanted)
                m_gain = wanted;
        }
        else if (m_gain < 1.0f)
        {
            m_gain += m_release;
            if (m_gain > 1.0f)
                m_gain = 1.0f;
        }

        for (int c = 0; c < channels; c++)
        {
            float v = pcm[i + c] * m_gain;

            // The gain ramp is not instant, so the first frames of a sudden
            // transient can still be over. Clamp them rather than let them
            // wrap in the conversion to integer samples.
            if (v > m_ceiling)
                v = m_ceiling;
            else if (v < -m_ceiling)
                v = -m_ceiling;

            pcm[i + c] = v;
        }
    }
}

Limiter &limiter()
{
    return g_limiter;
}

}
