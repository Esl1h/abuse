#include <doctest/doctest.h>

#include <stdio.h>

#include "common.h"
#include "image.h"
#include "ui/hexfont.h"

#ifndef ABUSE_TEST_DATA_DIR
#   define ABUSE_TEST_DATA_DIR "data"
#endif

using namespace abuse::ui;

namespace {

char const *font_path()
{
    static char path[512];
    snprintf(path, sizeof(path), "%s/fonts/unscii-8-thin.hex", ABUSE_TEST_DATA_DIR);
    return path;
}

HexFont const &font()
{
    static HexFont f;
    static bool loaded = f.Load(font_path());
    REQUIRE(loaded);
    return f;
}

bool cell_has_ink(image *im, int c, uint8_t ink)
{
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (im->Pixel(ivec2((c % 32) * 8 + x, (c / 32) * 8 + y)) == ink)
                return true;
    return false;
}

}

TEST_CASE("the vendored font loads") {
    CHECK(font().Size() == 3190);
    CHECK_FALSE(font().Empty());
}

// The .hex format also carries taller glyphs, and one is in this file. A cell
// of a different height has no place in a fixed 8x8 atlas.
TEST_CASE("glyphs that are not 8x8 are left out") {
    CHECK(font().Find(0x0000) == nullptr);
    CHECK(font().Find('A') != nullptr);
}

TEST_CASE("a codepoint the font does not have returns nothing") {
    CHECK(font().Find(0x10FFFF) == nullptr);
}

TEST_CASE("the accented letters Portuguese needs are present") {
    for (uint32_t cp : { 0xE3u, 0xF5u, 0xE7u, 0xE1u, 0xE9u, 0xEAu, 0xF3u, 0xF4u,
                         0xC3u, 0xD5u, 0xC7u })
        CHECK(font().Find(cp) != nullptr);
}

TEST_CASE("the CP437 table maps the bytes the shipped translations use") {
    uint16_t const *cp = codepage_table(Codepage::CP437);
    CHECK(cp[0x82] == 0xE9);    // e acute, as in french.lsp
    CHECK(cp[0x81] == 0xFC);    // u umlaut, as in german.lsp
    CHECK(cp[0xE1] == 0xDF);    // sharp s
    CHECK(cp[0x41] == 0x41);    // ASCII is itself
    // What makes CP437 unusable for Portuguese: no a-tilde anywhere in it.
    bool has_atilde = false;
    for (int i = 0; i < 256; i++)
        if (cp[i] == 0xE3)
            has_atilde = true;
    CHECK_FALSE(has_atilde);
}

TEST_CASE("the Latin-1 table is the identity") {
    uint16_t const *cp = codepage_table(Codepage::Latin1);
    for (int i = 0; i < 256; i++)
        CHECK(cp[i] == i);
}

TEST_CASE("the atlas has the shape JCFont cuts up") {
    image *im = build_atlas(font(), Codepage::Latin1, kFontInk);
    REQUIRE(im != nullptr);
    // 32 glyphs across and 8 down, which is how JCFont derives its cell size.
    CHECK(im->Size().x == 8 * 32);
    CHECK(im->Size().y == 8 * 8);
    delete im;
}

TEST_CASE("the Latin-1 atlas can spell Portuguese") {
    image *im = build_atlas(font(), Codepage::Latin1, kFontInk);
    REQUIRE(im != nullptr);
    CHECK(cell_has_ink(im, 'A', kFontInk));
    CHECK(cell_has_ink(im, 0xE3, kFontInk));    // a tilde
    CHECK(cell_has_ink(im, 0xF5, kFontInk));    // o tilde
    CHECK(cell_has_ink(im, 0xE7, kFontInk));    // c cedilla
    // Space carries no ink, and neither does a slot with no glyph.
    CHECK_FALSE(cell_has_ink(im, ' ', kFontInk));
    delete im;
}

// Same byte, different letter: this is why the codepage travels with the
// language instead of being global.
TEST_CASE("the two atlases differ where the codepages differ") {
    image *latin = build_atlas(font(), Codepage::Latin1, kFontInk);
    image *dos = build_atlas(font(), Codepage::CP437, kFontInk);
    REQUIRE(latin != nullptr);
    REQUIRE(dos != nullptr);

    bool same = true;
    for (int y = 0; y < 8 && same; y++)
        for (int x = 0; x < 8 && same; x++)
            if (latin->Pixel(ivec2(0xE3 % 32 * 8 + x, 0xE3 / 32 * 8 + y))
                != dos->Pixel(ivec2(0xE3 % 32 * 8 + x, 0xE3 / 32 * 8 + y)))
                same = false;
    CHECK_FALSE(same);

    // ASCII is the same in both, so the English text does not move.
    for (int c = 32; c < 127; c++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                REQUIRE(latin->Pixel(ivec2((c % 32) * 8 + x, (c / 32) * 8 + y))
                        == dos->Pixel(ivec2((c % 32) * 8 + x, (c / 32) * 8 + y)));

    delete latin;
    delete dos;
}

TEST_CASE("the atlas is painted in the ink it was asked for") {
    image *im = build_atlas(font(), Codepage::Latin1, 42);
    REQUIRE(im != nullptr);
    CHECK(cell_has_ink(im, 'A', 42));
    CHECK_FALSE(cell_has_ink(im, 'A', kFontInk));
    delete im;
}

TEST_CASE("a font file that is not there is reported, not crashed on") {
    HexFont f;
    CHECK_FALSE(f.Load("fonts/there-is-no-such-font.hex"));
    CHECK(f.Empty());
}

TEST_CASE("the font choice parses both names and rejects the rest") {
    bool extended = false;
    REQUIRE(parse_font_choice("extended", extended));
    CHECK(extended);
    REQUIRE(parse_font_choice("classic", extended));
    CHECK_FALSE(extended);
    CHECK_FALSE(parse_font_choice("gothic", extended));
    CHECK_FALSE(parse_font_choice(nullptr, extended));
    CHECK_FALSE(extended);
}
