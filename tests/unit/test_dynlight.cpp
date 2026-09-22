/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Unit tests for the light-emitting object table (phase 6, block 6.4).
 *
 *  This software was released into the Public Domain.
 */

#include <doctest/doctest.h>

#include "render/dynlight.h"
#include "render/lightmap.h"

using namespace abuse::render;

TEST_CASE("the table reads a name and two numbers")
{
    std::vector<Emitter> out;
    CHECK(parse_emitters("ROCKET 48 28\n", out) == 0);
    REQUIRE(out.size() == 1);
    CHECK(out[0].name == "ROCKET");
    CHECK(out[0].radius == 48);
    CHECK(out[0].strength == 28);
}

TEST_CASE("comments, blank lines and stray spacing are not entries")
{
    std::vector<Emitter> out;
    char const *text = "# a comment\n"
                       "\n"
                       "   \t \n"
                       "  GRENADE   30   14   # trailing comment\n"
                       "\n";
    CHECK(parse_emitters(text, out) == 0);
    REQUIRE(out.size() == 1);
    CHECK(out[0].name == "GRENADE");
    CHECK(out[0].radius == 30);
}

TEST_CASE("a line missing a number is dropped and counted")
{
    std::vector<Emitter> out;
    CHECK(parse_emitters("ROCKET 48\nGRENADE 30 14\nMBULLET\n", out) == 2);
    REQUIRE(out.size() == 1);
    CHECK(out[0].name == "GRENADE");
}

TEST_CASE("a light with no reach or no strength is not a light")
{
    std::vector<Emitter> out;
    CHECK(parse_emitters("A 0 20\nB 30 0\nC -5 20\n", out) == 3);
    CHECK(out.empty());
}

TEST_CASE("strength is capped at full brightness")
{
    std::vector<Emitter> out;
    parse_emitters("A 10 500\n", out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].strength == kFullLight);
}

TEST_CASE("a last line without a newline still counts")
{
    std::vector<Emitter> out;
    parse_emitters("A 10 20", out);
    CHECK(out.size() == 1);
}

TEST_CASE("brightening adds most at the centre and nothing at the edge")
{
    LightMap map;
    map.resize(40, 40);
    for (int y = 0; y < 40; y++)
        map.fill(0, y, 40, 0);

    brighten(map, 20, 20, 10, 30, 30, 30);

    CHECK(map.at(20, 20).r == 30);
    CHECK(map.at(25, 20).r == 15);        // half way out, half the strength
    CHECK(map.at(30, 20).r == 0);         // at the radius, nothing
    CHECK(map.at(31, 20).r == 0);         // and past it
    CHECK(map.at(20, 0).r == 0);
}

TEST_CASE("brightening adds to what is there and stops at full")
{
    LightMap map;
    map.resize(20, 20);
    for (int y = 0; y < 20; y++)
        map.fill(0, y, 20, 50);

    brighten(map, 10, 10, 8, 30, 30, 30);

    CHECK(map.at(10, 10).r == kFullLight);   // 50 + 30 would be 80
    CHECK(map.at(17, 10).r > 50);            // still brighter than it was
    CHECK(map.at(18, 10).r == 50);           // outside the reach, untouched
}

TEST_CASE("a light off the edge of the map lights the part that is on it")
{
    LightMap map;
    map.resize(20, 20);
    for (int y = 0; y < 20; y++)
        map.fill(0, y, 20, 0);

    brighten(map, -5, 10, 10, 40, 40, 40);

    CHECK(map.at(0, 10).r > 0);
    CHECK(map.at(5, 10).r == 0);
}

TEST_CASE("an empty map, no reach and no strength are all no-ops")
{
    LightMap empty;
    brighten(empty, 0, 0, 10, 10, 10, 10);      // must not write anywhere

    LightMap map;
    map.resize(10, 10);
    for (int y = 0; y < 10; y++)
        map.fill(0, y, 10, 7);

    brighten(map, 5, 5, 0, 10, 10, 10);
    brighten(map, 5, 5, 10, 0, 0, 0);
    CHECK(map.at(5, 5).r == 7);
}

TEST_CASE("the colour and the waver are optional")
{
    std::vector<Emitter> out;
    parse_emitters("A 10 20\n", out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].r == 255);
    CHECK(out[0].g == 255);
    CHECK(out[0].b == 255);
    CHECK(out[0].flicker == 0);
}

TEST_CASE("a colour is read and clamped to a byte")
{
    std::vector<Emitter> out;
    parse_emitters("A 10 20 255 120 0\nB 10 20 900 -5 40\n", out);
    REQUIRE(out.size() == 2);
    CHECK(out[0].r == 255);
    CHECK(out[0].g == 120);
    CHECK(out[0].b == 0);
    CHECK(out[1].r == 255);
    CHECK(out[1].g == 0);
}

TEST_CASE("half a colour is a typo, not a light")
{
    std::vector<Emitter> out;
    CHECK(parse_emitters("A 10 20 255\nB 10 20 255 120\n", out) == 2);
    CHECK(out.empty());
}

TEST_CASE("the waver is read after the colour and capped at 100")
{
    std::vector<Emitter> out;
    parse_emitters("A 10 20 255 255 255 40\nB 10 20 1 2 3 400\n", out);
    REQUIRE(out.size() == 2);
    CHECK(out[0].flicker == 40);
    CHECK(out[1].flicker == 100);
}

TEST_CASE("a white steady light is its strength on every channel")
{
    Emitter e;
    e.strength = 30;

    int r = 0, g = 0, b = 0;
    emitter_strength(e, 7, 1, r, g, b);
    CHECK(r == 30);
    CHECK(g == 30);
    CHECK(b == 30);
}

TEST_CASE("a colour is a share of the strength per channel")
{
    Emitter e;
    e.strength = 40;
    e.r = 255; e.g = 128; e.b = 0;

    int r = 0, g = 0, b = 0;
    emitter_strength(e, 3, 2, r, g, b);
    CHECK(r == 40);
    CHECK(g == 20);
    CHECK(b == 0);
}

TEST_CASE("the waver stays under the strength and moves with the tick")
{
    Emitter e;
    e.strength = 40;
    e.flicker = 50;

    int lowest = 1000, highest = -1, changed = 0, last = -1;
    for (int tick = 0; tick < 200; tick++)
    {
        int r = 0, g = 0, b = 0;
        emitter_strength(e, tick, 1, r, g, b);

        // Never brighter than the table says, and never out the bottom.
        CHECK(r <= 40);
        CHECK(r >= 20);

        if (r < lowest) lowest = r;
        if (r > highest) highest = r;
        if (last >= 0 && r != last) changed++;
        last = r;
    }

    CHECK(highest > lowest);        // it does waver
    CHECK(changed > 100);           // and not once every twenty ticks
}

TEST_CASE("the waver is the same answer for the same tick")
{
    Emitter e;
    e.strength = 40;
    e.flicker = 60;

    int r1 = 0, g1 = 0, b1 = 0, r2 = 0, g2 = 0, b2 = 0;
    emitter_strength(e, 99, 5, r1, g1, b1);
    emitter_strength(e, 99, 5, r2, g2, b2);
    CHECK(r1 == r2);

    // Two objects of the same type on the same tick do not waver
    // together. Across a handful of seeds rather than one: there are only
    // a couple of dozen levels to land on, so any two can agree by chance.
    int differs = 0;
    for (int seed = 0; seed < 20; seed++)
    {
        int r3 = 0, g3 = 0, b3 = 0;
        emitter_strength(e, 99, seed, r3, g3, b3);
        if (r3 != r1)
            differs++;
    }
    CHECK(differs > 10);
}
