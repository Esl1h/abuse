#include <doctest/doctest.h>

#include <stdio.h>

#include "ui/hexfont.h"
#include "ui/overlay.h"

#ifndef ABUSE_TEST_DATA_DIR
#   define ABUSE_TEST_DATA_DIR "data"
#endif

using namespace abuse::ui;

namespace {

HexFont const &font()
{
    static HexFont f;
    static bool loaded = false;
    if (!loaded)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/fonts/unscii-8-thin.hex",
                 ABUSE_TEST_DATA_DIR);
        loaded = f.Load(path);
    }
    REQUIRE(!f.Empty());
    return f;
}

uint32_t pixel(Overlay const &o, int x, int y)
{
    return o.Pixels()[(size_t)y * (size_t)o.Width() + (size_t)x];
}

int ink_count(Overlay const &o)
{
    int n = 0;
    for (int y = 0; y < o.Height(); y++)
        for (int x = 0; x < o.Width(); x++)
            if (pixel(o, x, y) >> 24)
                n++;
    return n;
}

}

TEST_CASE("a fresh overlay is empty and transparent") {
    Overlay o;
    REQUIRE(o.Begin(64, 32));
    CHECK(o.Width() == 64);
    CHECK(o.Height() == 32);
    CHECK_FALSE(o.Dirty());
    CHECK(ink_count(o) == 0);
}

// The game clears it every frame, including before anything has sized it.
TEST_CASE("clearing an overlay that was never sized is harmless") {
    Overlay o;
    o.Clear();
    CHECK(o.Empty());
    CHECK_FALSE(o.Dirty());
}

TEST_CASE("a size that makes no sense is refused") {
    Overlay o;
    CHECK_FALSE(o.Begin(0, 32));
    CHECK_FALSE(o.Begin(64, -1));
    CHECK(o.Empty());
}

TEST_CASE("resizing keeps working") {
    Overlay o;
    REQUIRE(o.Begin(64, 32));
    o.FillRect(0, 0, 64, 32, rgba(255, 0, 0));
    REQUIRE(o.Begin(128, 64));
    CHECK(o.Width() == 128);
    CHECK(ink_count(o) == 0);
}

TEST_CASE("an opaque fill lands where it was asked to") {
    Overlay o;
    REQUIRE(o.Begin(64, 32));
    o.FillRect(10, 5, 4, 3, rgba(255, 0, 0));
    CHECK(o.Dirty());
    CHECK(pixel(o, 10, 5) == rgba(255, 0, 0));
    CHECK(pixel(o, 13, 7) == rgba(255, 0, 0));
    CHECK(pixel(o, 14, 7) == 0);
    CHECK(pixel(o, 9, 5) == 0);
    CHECK(ink_count(o) == 12);
}

// Drawing off the edge has to be ignored, not wrapped and not written outside
// the buffer: panels are positioned from the window size, which changes.
TEST_CASE("drawing outside the overlay is dropped") {
    Overlay o;
    REQUIRE(o.Begin(16, 16));
    o.FillRect(-4, -4, 8, 8, rgba(255, 255, 255));
    CHECK(pixel(o, 0, 0) == rgba(255, 255, 255));
    CHECK(pixel(o, 3, 3) == rgba(255, 255, 255));
    CHECK(pixel(o, 4, 4) == 0);

    o.Clear();
    o.FillRect(14, 14, 8, 8, rgba(255, 255, 255));
    CHECK(pixel(o, 15, 15) == rgba(255, 255, 255));
    CHECK(ink_count(o) == 4);
}

TEST_CASE("a fully transparent colour changes nothing") {
    Overlay o;
    REQUIRE(o.Begin(16, 16));
    o.FillRect(0, 0, 16, 16, rgba(255, 0, 0, 0));
    CHECK_FALSE(o.Dirty());
    CHECK(ink_count(o) == 0);
}

// A half transparent panel over the game is the whole point of blending here.
TEST_CASE("a half transparent colour mixes with what is under it") {
    Overlay o;
    REQUIRE(o.Begin(16, 16));
    o.FillRect(0, 0, 16, 16, rgba(0, 0, 0));
    o.FillRect(0, 0, 4, 4, rgba(255, 255, 255, 128));

    uint32_t mixed = pixel(o, 0, 0);
    uint32_t r = (mixed >> 16) & 0xff;
    CHECK(r > 100);
    CHECK(r < 160);
    CHECK((mixed >> 24) == 255);
}

TEST_CASE("a frame is hollow") {
    Overlay o;
    REQUIRE(o.Begin(32, 32));
    o.FrameRect(2, 2, 10, 6, rgba(0, 255, 0));
    CHECK(pixel(o, 2, 2) == rgba(0, 255, 0));
    CHECK(pixel(o, 11, 7) == rgba(0, 255, 0));
    CHECK(pixel(o, 5, 4) == 0);
    // Two rows of 10 plus two columns of 4.
    CHECK(ink_count(o) == 10 * 2 + 4 * 2);
}

TEST_CASE("text is drawn and advances by the cell width") {
    Overlay o;
    REQUIRE(o.Begin(128, 32));
    int end = o.Text(font(), 0, 0, "Abuse", 1, rgba(255, 255, 255));
    CHECK(end == 5 * 8);
    CHECK(end == Overlay::TextWidth("Abuse", 1));
    CHECK(o.Dirty());
    CHECK(ink_count(o) > 0);
}

// The reason this layer exists: at a larger window the letters get bigger
// without getting blurry, so one font pixel has to become a solid block.
TEST_CASE("scaling multiplies the ink instead of interpolating it") {
    Overlay one, two;
    REQUIRE(one.Begin(256, 64));
    REQUIRE(two.Begin(256, 64));

    one.Text(font(), 0, 0, "Abuse", 1, rgba(255, 255, 255));
    two.Text(font(), 0, 0, "Abuse", 2, rgba(255, 255, 255));

    CHECK(ink_count(two) == ink_count(one) * 4);
    CHECK(Overlay::TextWidth("Abuse", 2) == Overlay::TextWidth("Abuse", 1) * 2);
    CHECK(Overlay::TextHeight(2) == 16);

    // Every pixel of the scaled text is either the colour or nothing: an
    // interpolated one would have shades in between.
    for (int y = 0; y < two.Height(); y++)
        for (int x = 0; x < two.Width(); x++)
        {
            uint32_t p = pixel(two, x, y);
            REQUIRE((p == 0 || p == rgba(255, 255, 255)));
        }
}

// Latin-1 straight through: the overlay is the one surface that has to be
// able to spell "Opcoes" with its cedilla and its tilde.
TEST_CASE("accented text draws, and differs from the unaccented spelling") {
    Overlay plain, accented;
    REQUIRE(plain.Begin(128, 32));
    REQUIRE(accented.Begin(128, 32));

    plain.Text(font(), 0, 0, "Opcoes", 1, rgba(255, 255, 255));
    accented.Text(font(), 0, 0, "Op\xe7\xf5" "es", 1, rgba(255, 255, 255));

    CHECK(ink_count(plain) > 0);
    CHECK(ink_count(accented) > 0);

    bool same = true;
    for (int y = 0; y < 32 && same; y++)
        for (int x = 0; x < 128 && same; x++)
            if (pixel(plain, x, y) != pixel(accented, x, y))
                same = false;
    CHECK_FALSE(same);

    // The two spell the same number of letters, so they end at the same x.
    CHECK(Overlay::TextWidth("Opcoes", 1)
          == Overlay::TextWidth("Op\xe7\xf5" "es", 1));
}

TEST_CASE("null and empty text are harmless") {
    Overlay o;
    REQUIRE(o.Begin(32, 32));
    CHECK(o.Text(font(), 4, 4, nullptr, 1, rgba(255, 255, 255)) == 4);
    CHECK(o.Text(font(), 4, 4, "", 1, rgba(255, 255, 255)) == 4);
    CHECK(o.Text(font(), 4, 4, "x", 0, rgba(255, 255, 255)) == 4);
    CHECK_FALSE(o.Dirty());
    CHECK(Overlay::TextWidth(nullptr, 1) == 0);
}

TEST_CASE("the pitch matches the width") {
    Overlay o;
    REQUIRE(o.Begin(100, 20));
    CHECK(o.Pitch() == 100 * 4);
}
