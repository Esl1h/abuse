#include <doctest/doctest.h>

#include <initializer_list>

#include "input/rumble.h"

using namespace abuse::input;

TEST_CASE("each event has its own feel") {
    RumbleSettings s;   // 100%

    RumbleCommand shot = rumble_for(RumbleEvent::Shot, s);
    RumbleCommand hurt = rumble_for(RumbleEvent::Hurt, s);
    RumbleCommand boom = rumble_for(RumbleEvent::Explosion, s);

    CHECK_FALSE(shot.silent());
    CHECK_FALSE(hurt.silent());
    CHECK_FALSE(boom.silent());

    // A shot fires many times a second; it has to be the shortest, or the pad
    // buzzes continuously instead of ticking.
    CHECK(shot.duration_ms < hurt.duration_ms);
    CHECK(hurt.duration_ms < boom.duration_ms);

    // And an explosion is the heaviest on the low motor.
    CHECK(shot.low < hurt.low);
    CHECK(hurt.low < boom.low);
}

// The setting a player reaches for first has to actually turn it off.
TEST_CASE("zero strength is silent for every event") {
    RumbleSettings s;
    s.strength = 0;

    for (RumbleEvent e : { RumbleEvent::Shot, RumbleEvent::Hurt, RumbleEvent::Explosion })
    {
        RumbleCommand c = rumble_for(e, s);
        CHECK(c.silent());
        CHECK(c.low == 0);
        CHECK(c.high == 0);
    }
}

TEST_CASE("strength scales the motors and not the duration") {
    RumbleSettings full;
    RumbleSettings half;
    half.strength = 50;

    RumbleCommand a = rumble_for(RumbleEvent::Hurt, full);
    RumbleCommand b = rumble_for(RumbleEvent::Hurt, half);

    CHECK(b.low == a.low / 2);
    CHECK(b.high == a.high / 2);
    // Halving how long it lasts would change what the event reads as, not how
    // strong it feels.
    CHECK(b.duration_ms == a.duration_ms);
}

TEST_CASE("motor values stay inside the 16-bit range") {
    RumbleSettings s;
    for (int strength = 0; strength <= 100; strength++)
    {
        s.strength = strength;
        for (RumbleEvent e : { RumbleEvent::Shot, RumbleEvent::Hurt, RumbleEvent::Explosion })
        {
            RumbleCommand c = rumble_for(e, s);
            CHECK(c.low <= 0xffff);
            CHECK(c.high <= 0xffff);
        }
    }
}

TEST_CASE("strength parses from the config") {
    int v = -1;
    REQUIRE(parse_rumble_strength("0", v));
    CHECK(v == 0);
    REQUIRE(parse_rumble_strength("100", v));
    CHECK(v == 100);
    REQUIRE(parse_rumble_strength("55", v));
    CHECK(v == 55);
}

TEST_CASE("invalid strength leaves the default alone") {
    int v = 100;
    CHECK_FALSE(parse_rumble_strength("101", v));
    CHECK_FALSE(parse_rumble_strength("-5", v));
    CHECK_FALSE(parse_rumble_strength("abc", v));
    CHECK_FALSE(parse_rumble_strength("50%", v));
    CHECK_FALSE(parse_rumble_strength("", v));
    CHECK_FALSE(parse_rumble_strength(nullptr, v));
    CHECK(v == 100);
}
