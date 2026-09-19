/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Unit tests for the mix buses and the voice policy (phase 5, task 5.1).
 *
 *  This software was released into the Public Domain.
 */

#include <doctest/doctest.h>

#include "audio/buses.h"
#include "audio/limiter.h"
#include "audio/voices.h"

using abuse::audio::Bus;
using abuse::audio::VoicePool;

namespace {

// The buses are process-wide, like the render options. Every case that
// changes them puts them back.
struct BusReset
{
    ~BusReset()
    {
        abuse::audio::set_master(1.0f);
        for (int i = 0; i < abuse::audio::kBusCount; i++)
            abuse::audio::set_gain((Bus)i, 1.0f);
    }
};

}

TEST_CASE("a fresh mix changes nothing") {
    BusReset reset;

    CHECK(abuse::audio::master() == doctest::Approx(1.0f));
    for (int i = 0; i < abuse::audio::kBusCount; i++)
        CHECK(abuse::audio::gain((Bus)i) == doctest::Approx(1.0f));

    // The engine's own scale, 0 to 255, arrives unchanged.
    CHECK(abuse::audio::voice_gain(Bus::Sfx, 255) == doctest::Approx(1.0f));
    CHECK(abuse::audio::voice_gain(Bus::Sfx, 128) == doctest::Approx(128.0f / 255.0f));
    CHECK(abuse::audio::voice_gain(Bus::Sfx, 0) == doctest::Approx(0.0f));
}

TEST_CASE("a bus and the master multiply, and a voice never exceeds one") {
    BusReset reset;

    abuse::audio::set_gain(Bus::Music, 0.5f);
    CHECK(abuse::audio::voice_gain(Bus::Music, 255) == doctest::Approx(0.5f));

    abuse::audio::set_master(0.5f);
    CHECK(abuse::audio::voice_gain(Bus::Music, 255) == doctest::Approx(0.25f));

    // Other buses are untouched by a change to one of them.
    CHECK(abuse::audio::voice_gain(Bus::Sfx, 255) == doctest::Approx(0.5f));

    // Lifting a quiet bus is allowed; clipping a single voice is not.
    abuse::audio::set_master(1.0f);
    abuse::audio::set_gain(Bus::Sfx, 2.0f);
    CHECK(abuse::audio::voice_gain(Bus::Sfx, 255) == doctest::Approx(1.0f));
    CHECK(abuse::audio::voice_gain(Bus::Sfx, 64) == doctest::Approx(0.5019f).epsilon(0.01));

    // And the range is capped rather than trusted.
    abuse::audio::set_gain(Bus::Sfx, 9.0f);
    CHECK(abuse::audio::gain(Bus::Sfx) == doctest::Approx(2.0f));
    abuse::audio::set_gain(Bus::Sfx, -1.0f);
    CHECK(abuse::audio::gain(Bus::Sfx) == doctest::Approx(0.0f));
}

TEST_CASE("out of range volumes are clamped, not wrapped") {
    BusReset reset;
    CHECK(abuse::audio::voice_gain(Bus::Sfx, -5) == doctest::Approx(0.0f));
    CHECK(abuse::audio::voice_gain(Bus::Sfx, 4000) == doctest::Approx(1.0f));
}

TEST_CASE("bus names round trip, and a typo is refused") {
    Bus b = Bus::Music;
    CHECK(abuse::audio::parse_bus("sfx", b));
    CHECK(b == Bus::Sfx);
    CHECK(abuse::audio::parse_bus("MUSIC", b));
    CHECK(b == Bus::Music);
    CHECK(abuse::audio::parse_bus("ui", b));
    CHECK(b == Bus::Ui);

    Bus keep = Bus::Ui;
    CHECK_FALSE(abuse::audio::parse_bus("speech", keep));
    CHECK(keep == Bus::Ui);
    CHECK_FALSE(abuse::audio::parse_bus(nullptr, keep));
}

TEST_CASE("the percentage in abuserc is the scale a person reads") {
    float g = 1.0f;
    CHECK(abuse::audio::parse_percent("0", g));
    CHECK(g == doctest::Approx(0.0f));
    CHECK(abuse::audio::parse_percent("100", g));
    CHECK(g == doctest::Approx(1.0f));
    CHECK(abuse::audio::parse_percent("55", g));
    CHECK(g == doctest::Approx(0.55f));

    CHECK(abuse::audio::percent(0.55f) == 55);
    CHECK(abuse::audio::percent(1.0f) == 100);

    g = 0.75f;
    CHECK_FALSE(abuse::audio::parse_percent("loud", g));
    CHECK_FALSE(abuse::audio::parse_percent("-10", g));
    CHECK_FALSE(abuse::audio::parse_percent("500", g));
    CHECK_FALSE(abuse::audio::parse_percent("50%", g));
    CHECK(g == doctest::Approx(0.75f));
}

TEST_CASE("a free voice is used before any is taken") {
    VoicePool pool;
    pool.reset(3);
    CHECK(pool.size() == 3);

    CHECK(pool.acquire(abuse::audio::kNormal, 0) == 0);
    CHECK(pool.acquire(abuse::audio::kNormal, 1) == 1);
    CHECK(pool.acquire(abuse::audio::kNormal, 2) == 2);
    CHECK(pool.busy(0));
    CHECK(pool.busy(2));

    // Full, and nothing outranks what is playing.
    CHECK(pool.acquire(abuse::audio::kNormal, 3) == VoicePool::kNone);

    pool.release(1);
    CHECK_FALSE(pool.busy(1));
    CHECK(pool.acquire(abuse::audio::kNormal, 4) == 1);
}

TEST_CASE("a louder sound takes the weakest voice, and the oldest among equals") {
    VoicePool pool;
    pool.reset(3);

    pool.acquire(abuse::audio::kNormal, 10);     // slot 0
    pool.acquire(abuse::audio::kAmbient, 20);    // slot 1, the weakest
    pool.acquire(abuse::audio::kNormal, 30);     // slot 2

    CHECK(pool.acquire(abuse::audio::kImportant, 40) == 1);
    CHECK(pool.priority_of(1) == abuse::audio::kImportant);

    // Now the two normals are the weakest, and slot 0 started first.
    CHECK(pool.acquire(abuse::audio::kUi, 50) == 0);

    // Everything left outranks an ambient loop, so it is simply dropped.
    CHECK(pool.acquire(abuse::audio::kAmbient, 60) == VoicePool::kNone);
}

TEST_CASE("equal priority keeps what is already sounding") {
    VoicePool pool;
    pool.reset(2);

    pool.acquire(abuse::audio::kNormal, 0);
    pool.acquire(abuse::audio::kNormal, 1);

    // Otherwise a burst of identical shots would cut itself off, each one
    // silencing the one before.
    CHECK(pool.acquire(abuse::audio::kNormal, 2) == VoicePool::kNone);
    CHECK(pool.priority_of(0) == abuse::audio::kNormal);
}

TEST_CASE("a pool with no voices answers instead of crashing") {
    VoicePool pool;
    CHECK(pool.size() == 0);
    CHECK(pool.acquire(abuse::audio::kUi, 0) == VoicePool::kNone);
    CHECK_FALSE(pool.busy(0));
    pool.release(0);
    pool.release(-1);
    pool.release_all();

    pool.reset(-3);
    CHECK(pool.size() == 0);
}

// ---- the master limiter ---------------------------------------------------

namespace {

// Peak of a buffer, which is what the limiter is judged by.
float peak_of(float const *pcm, int n)
{
    float peak = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float v = pcm[i] < 0.0f ? -pcm[i] : pcm[i];
        if (v > peak)
            peak = v;
    }
    return peak;
}

// A block of stereo frames all at the same level, which is the simplest
// signal that says whether the gain settled where it should.
void fill(float *pcm, int n, float level)
{
    for (int i = 0; i < n; i++)
        pcm[i] = level;
}

}

TEST_CASE("the limiter leaves a quiet mix alone") {
    abuse::audio::Limiter lim;
    lim.configure(44100, 2);

    float pcm[512];
    fill(pcm, 512, 0.5f);
    lim.process(pcm, 512);

    for (int i = 0; i < 512; i++)
        CHECK(pcm[i] == doctest::Approx(0.5f));
    CHECK(lim.current_gain() == doctest::Approx(1.0f));
}

TEST_CASE("a mix over the ceiling comes back under it") {
    abuse::audio::Limiter lim;
    lim.configure(44100, 2);
    lim.set_ceiling(0.9f);

    // Two seconds of a signal at nearly three times the ceiling. Nothing may
    // leave above it, ramp or no ramp.
    float pcm[4096];
    for (int block = 0; block < 20; block++)
    {
        fill(pcm, 4096, 2.5f);
        lim.process(pcm, 4096);
        CHECK(peak_of(pcm, 4096) <= 0.9f + 1e-5f);
    }

    // And it has settled where it needs to be, not somewhere below.
    CHECK(lim.current_gain() == doctest::Approx(0.9f / 2.5f).epsilon(0.05));
}

TEST_CASE("the gain comes back up after the loud part") {
    abuse::audio::Limiter lim;
    lim.configure(44100, 2);

    float pcm[4096];
    fill(pcm, 4096, 4.0f);
    lim.process(pcm, 4096);
    float held_down = lim.current_gain();
    CHECK(held_down < 0.5f);

    // Half a second of quiet afterwards.
    for (int block = 0; block < 6; block++)
    {
        fill(pcm, 4096, 0.1f);
        lim.process(pcm, 4096);
    }
    CHECK(lim.current_gain() > held_down);
    CHECK(lim.current_gain() == doctest::Approx(1.0f).epsilon(0.001));
}

TEST_CASE("turning it off means the buffer is not touched") {
    abuse::audio::Limiter lim;
    lim.configure(44100, 2);
    lim.set_enabled(false);

    float pcm[64];
    fill(pcm, 64, 3.0f);
    lim.process(pcm, 64);

    // Including the values over full scale: the Original mode's sound is the
    // reference, clipping and all.
    for (int i = 0; i < 64; i++)
        CHECK(pcm[i] == doctest::Approx(3.0f));
}

TEST_CASE("both channels move together, so the image does not shift") {
    abuse::audio::Limiter lim;
    lim.configure(44100, 2);
    lim.set_ceiling(0.8f);

    // Loud on the left, quiet on the right, for long enough to settle.
    float pcm[4096];
    for (int block = 0; block < 10; block++)
    {
        for (int i = 0; i < 4096; i += 2)
        {
            pcm[i] = 2.0f;
            pcm[i + 1] = 1.0f;
        }
        lim.process(pcm, 4096);
    }

    // The ratio between the channels survives; only the level changed.
    CHECK(pcm[0] / pcm[1] == doctest::Approx(2.0f).epsilon(0.01));
    CHECK(peak_of(pcm, 4096) <= 0.8f + 1e-5f);
}

TEST_CASE("a limiter asked for nonsense still behaves") {
    abuse::audio::Limiter lim;
    lim.configure(0, 0);

    lim.set_ceiling(-1.0f);
    CHECK(lim.ceiling() >= 0.01f);
    lim.set_ceiling(50.0f);
    CHECK(lim.ceiling() == doctest::Approx(1.0f));

    lim.process(nullptr, 128);      // must not crash
    float one = 0.5f;
    lim.process(&one, 0);
    CHECK(one == doctest::Approx(0.5f));
}
