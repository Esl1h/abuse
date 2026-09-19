/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Master limiter. Phase 5, task 5.1.
 *
 *  Abuse fires a lot of short, loud sounds at once: a grenade, the four
 *  things it kills, and the ambient loop under them. Each voice is inside
 *  full scale on its own, and their sum is not, so the output clips: the
 *  crackle that shows up exactly when the most is happening.
 *
 *  This rides the whole mix down when it would exceed the ceiling and lets it
 *  back up gently afterwards. It is the last thing in the chain, after every
 *  bus.
 *
 *  Free of SDL: a buffer of floats in, the same buffer out, so what it does
 *  to a signal can be measured in a test rather than listened for.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_AUDIO_LIMITER_H_
#define ABUSE_AUDIO_LIMITER_H_

namespace abuse::audio {

class Limiter
{
public:
    // `rate` is the sample rate, needed because attack and release are in
    // milliseconds and a buffer is in samples.
    void configure(int rate, int channels);

    // The ceiling, in linear amplitude. Just under 1 by default: a sample at
    // exactly full scale is where some hardware starts to distort.
    void set_ceiling(float ceiling);
    float ceiling() const { return m_ceiling; }

    // Off means the buffer is not touched at all, which is what the Original
    // mode uses: its sound is the reference, clipping included.
    void set_enabled(bool on);
    bool enabled() const { return m_enabled; }

    // In place, interleaved, `samples` floats in total across all channels.
    void process(float *pcm, int samples);

    // The gain it is currently applying, 1 when nothing needs holding down.
    // Exposed for the tests and for a meter, should one ever exist.
    float current_gain() const { return m_gain; }

    void reset();

private:
    bool m_enabled = true;
    float m_ceiling = 0.97f;
    float m_gain = 1.0f;

    // Per-sample coefficients, worked out from the rate in configure().
    float m_attack = 0.25f;
    float m_release = 0.0001f;
    int m_channels = 2;
};

// The one the backend installs. Settings reach it from abuserc, which is read
// before the mixer exists.
//
// Its process() runs on the audio thread. Everything else here is called from
// the main thread at startup, before the callback is installed, which is why
// none of it is synchronised. A control that toggles this while the game is
// running needs to think about that first.
Limiter &limiter();

}

#endif
