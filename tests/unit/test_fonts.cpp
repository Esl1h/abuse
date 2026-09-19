#include <doctest/doctest.h>

#include "common.h"
#include "fonts.h"
#include "image.h"

extern unsigned char fnt6x13[192 * 104];

// The engine builds its font exactly this way, from a 32x8 grid of glyphs.
static JCFont *make_font()
{
    image letters(ivec2(192, 104), fnt6x13);
    return new JCFont(&letters);
}

// French and German text carries bytes above 0x7f. char is signed on x86, so
// indexing the glyph array with one used to reach before the array.
TEST_CASE("the high half of the character set is reachable") {
    JCFont *font = make_font();
    image screen(ivec2(64, 32));
    screen.clear();

    for (int c = 128; c < 256; c++)
        font->PutChar(&screen, ivec2(0, 0), (char)c, 2);

    delete font;
}

TEST_CASE("an accented string draws without leaving the image") {
    JCFont *font = make_font();
    image screen(ivec2(64, 32));
    screen.clear();

    // "Delai" the way french.lsp spells it, in the codepage the data uses.
    font->PutString(&screen, ivec2(0, 0), "D\x82lai", 2);

    delete font;
}
