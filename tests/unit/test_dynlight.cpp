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

    brighten(map, 20, 20, 10, 30);

    CHECK(map.at(20, 20) == 30);
    CHECK(map.at(25, 20) == 15);        // half way out, half the strength
    CHECK(map.at(30, 20) == 0);         // at the radius, nothing
    CHECK(map.at(31, 20) == 0);         // and past it
    CHECK(map.at(20, 0) == 0);
}

TEST_CASE("brightening adds to what is there and stops at full")
{
    LightMap map;
    map.resize(20, 20);
    for (int y = 0; y < 20; y++)
        map.fill(0, y, 20, 50);

    brighten(map, 10, 10, 8, 30);

    CHECK(map.at(10, 10) == kFullLight);   // 50 + 30 would be 80
    CHECK(map.at(17, 10) > 50);            // still brighter than it was
    CHECK(map.at(18, 10) == 50);           // outside the reach, untouched
}

TEST_CASE("a light off the edge of the map lights the part that is on it")
{
    LightMap map;
    map.resize(20, 20);
    for (int y = 0; y < 20; y++)
        map.fill(0, y, 20, 0);

    brighten(map, -5, 10, 10, 40);

    CHECK(map.at(0, 10) > 0);
    CHECK(map.at(5, 10) == 0);
}

TEST_CASE("an empty map, no reach and no strength are all no-ops")
{
    LightMap empty;
    brighten(empty, 0, 0, 10, 10);      // must not write anywhere

    LightMap map;
    map.resize(10, 10);
    for (int y = 0; y < 10; y++)
        map.fill(0, y, 10, 7);

    brighten(map, 5, 5, 0, 10);
    brighten(map, 5, 5, 10, 0);
    CHECK(map.at(5, 5) == 7);
}
