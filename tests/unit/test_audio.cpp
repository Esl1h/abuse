/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Unit tests for the mix buses and the voice policy (phase 5, task 5.1).
 *
 *  This software was released into the Public Domain.
 */

#include <doctest/doctest.h>

#include "audio/buses.h"
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
