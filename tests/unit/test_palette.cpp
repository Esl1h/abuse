#include <doctest/doctest.h>

#include "palette.h"

TEST_CASE("set and get round trip every entry") {
    palette p(256);
    for (int i = 0; i < 256; i++)
        p.set(i, (uint8_t)i, (uint8_t)(255 - i), (uint8_t)((i * 7) & 0xff));

    for (int i = 0; i < 256; i++)
    {
        uint8_t r, g, b;
        p.get(i, r, g, b);
        CHECK((int)r == i);
        CHECK((int)g == 255 - i);
        CHECK((int)b == ((i * 7) & 0xff));
        CHECK((int)p.red(i) == i);
        CHECK((int)p.green(i) == 255 - i);
        CHECK((int)p.blue(i) == ((i * 7) & 0xff));
    }
}

TEST_CASE("getquad packs as 0x00RRGGBB on a little endian host") {
    palette p(256);
    p.set(7, 0x12, 0x34, 0x56);
    CHECK(p.getquad(7) == 0x00123456u);

    p.set(8, 0xff, 0xff, 0xff);
    CHECK(p.getquad(8) == 0x00ffffffu);

    p.set(9, 0, 0, 0);
    CHECK(p.getquad(9) == 0u);
}

TEST_CASE("find_closest returns an exact match when there is one") {
    palette p(256);
    for (int i = 0; i < 256; i++)
        p.set(i, (uint8_t)i, (uint8_t)i, (uint8_t)i);

    CHECK(p.find_closest(0, 0, 0) == 0);
    CHECK(p.find_closest(128, 128, 128) == 128);
    CHECK(p.find_closest(255, 255, 255) == 255);
}

TEST_CASE("find_closest minimises squared distance") {
    palette p(256);
    // A palette with only three distinct colours; the rest is pure black, so
    // index 0 wins every tie by being first.
    for (int i = 0; i < 256; i++)
        p.set(i, 0, 0, 0);
    p.set(10, 255, 0, 0);
    p.set(20, 0, 255, 0);
    p.set(30, 0, 0, 255);

    CHECK(p.find_closest(250, 5, 5) == 10);
    CHECK(p.find_closest(5, 250, 5) == 20);
    CHECK(p.find_closest(5, 5, 250) == 30);
    CHECK(p.find_closest(1, 1, 1) == 0);
}

TEST_CASE("find_closest keeps the first of equally distant entries") {
    palette p(256);
    for (int i = 0; i < 256; i++)
        p.set(i, 0, 0, 0);
    p.set(5, 100, 0, 0);
    p.set(6, 100, 0, 0);

    // Determinism matters here: the lighting tables are built from this.
    CHECK(p.find_closest(100, 0, 0) == 5);
}

TEST_CASE("size accounts for the colour count prefix") {
    palette p(256);
    CHECK(p.pal_size() == 256);
    CHECK(p.size() == 256 * 3 + 2);
}

TEST_CASE("used flags start clear and can be set") {
    palette p(256);
    p.set_all_unused();
    CHECK(p.used(0) == 0);
    CHECK(p.used(255) == 0);

    p.set_used(42);
    CHECK(p.used(42) != 0);
    CHECK(p.used(41) == 0);
    CHECK(p.used(43) == 0);

    p.set_all_used();
    for (int i = 0; i < 256; i++)
        CHECK(p.used(i) != 0);
}
