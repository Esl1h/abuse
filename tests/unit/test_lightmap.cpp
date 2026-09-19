/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Unit tests for the light map and the darkening curve (phase 6, 6.2).
 *
 *  This software was released into the Public Domain.
 */

#include <doctest/doctest.h>

#include "render/lightmap.h"

using abuse::render::LightMap;
using abuse::render::kFullLight;

TEST_CASE("the curve is the one the 1995 table bakes") {
    // calc_light_table walks intensity from 63 down to 0 subtracting one from
    // each channel per step, so level 63 is the colour itself.
    CHECK(abuse::render::shade_channel(200, kFullLight) == 200);
    CHECK(abuse::render::shade_channel(200, 62) == 199);
    CHECK(abuse::render::shade_channel(200, 0) == 200 - 63);

    // And it stops at black rather than wrapping, which is the other half of
    // what the table does.
    CHECK(abuse::render::shade_channel(10, 0) == 0);
    CHECK(abuse::render::shade_channel(0, 0) == 0);
    CHECK(abuse::render::shade_channel(63, 0) == 0);
    CHECK(abuse::render::shade_channel(64, 0) == 1);
}

TEST_CASE("a level outside the range is treated as one inside it") {
    CHECK(abuse::render::shade_channel(100, 999) == 100);
    CHECK(abuse::render::shade_channel(100, -5) == 100 - 63);
}

TEST_CASE("shade packs opaque ARGB") {
    uint32_t c = abuse::render::shade(10, 20, 30, kFullLight);
    CHECK(c == 0xff0a141eu);

    uint32_t dark = abuse::render::shade(100, 100, 100, 32);
    CHECK(((dark >> 24) & 0xff) == 0xff);
    CHECK(((dark >> 16) & 0xff) == 100 - 31);
    CHECK(((dark >> 8) & 0xff) == 100 - 31);
    CHECK((dark & 0xff) == 100 - 31);
}

TEST_CASE("a fresh map is fully lit") {
    LightMap map;
    map.resize(8, 4);
    CHECK(map.width() == 8);
    CHECK(map.height() == 4);
    CHECK_FALSE(map.empty());

    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 8; x++)
            CHECK(map.at(x, y) == kFullLight);
}

TEST_CASE("a run is written where it is asked for and nowhere else") {
    LightMap map;
    map.resize(8, 2);

    map.fill(2, 1, 3, 10);
    CHECK(map.at(1, 1) == kFullLight);
    CHECK(map.at(2, 1) == 10);
    CHECK(map.at(3, 1) == 10);
    CHECK(map.at(4, 1) == 10);
    CHECK(map.at(5, 1) == kFullLight);

    // The row above is untouched.
    for (int x = 0; x < 8; x++)
        CHECK(map.at(x, 0) == kFullLight);

    map.clear();
    CHECK(map.at(3, 1) == kFullLight);
}

TEST_CASE("runs that fall off the edge are clipped, not wrapped") {
    LightMap map;
    map.resize(8, 2);

    // Off the right: the part inside is written, and the next row is not
    // touched, which is what wrapping would do.
    map.fill(6, 0, 10, 5);
    CHECK(map.at(6, 0) == 5);
    CHECK(map.at(7, 0) == 5);
    CHECK(map.at(0, 1) == kFullLight);

    // Off the left: the part inside still lands in the right place.
    map.fill(-2, 1, 4, 7);
    CHECK(map.at(0, 1) == 7);
    CHECK(map.at(1, 1) == 7);
    CHECK(map.at(2, 1) == kFullLight);

    // Entirely outside, in every direction.
    map.fill(-10, 0, 3, 1);
    map.fill(20, 0, 3, 1);
    map.fill(0, -1, 3, 1);
    map.fill(0, 99, 3, 1);
    CHECK(map.at(0, 0) == kFullLight);
    CHECK(map.at(7, 0) == 5);
}

TEST_CASE("a level outside the range is clamped on the way in") {
    LightMap map;
    map.resize(4, 1);
    map.fill(0, 0, 1, 500);
    map.fill(1, 0, 1, -7);
    CHECK(map.at(0, 0) == kFullLight);
    CHECK(map.at(1, 0) == 0);
}

TEST_CASE("an empty map answers instead of crashing") {
    LightMap map;
    CHECK(map.empty());
    CHECK(map.at(0, 0) == kFullLight);
    CHECK(map.row(0) == nullptr);
    map.fill(0, 0, 4, 0);       // must not write anywhere
    CHECK(map.at(0, 0) == kFullLight);

    map.resize(-4, -4);
    CHECK(map.empty());
}

TEST_CASE("rows are contiguous, which is what the conversion walks") {
    LightMap map;
    map.resize(4, 3);
    map.fill(0, 2, 4, 9);

    uint8_t const *row = map.row(2);
    REQUIRE(row != nullptr);
    for (int x = 0; x < 4; x++)
        CHECK(row[x] == 9);

    CHECK(map.row(3) == nullptr);
    CHECK(map.row(-1) == nullptr);
}
