/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  An extended glyph set for the engine's fonts. Phase 4, task 4.2.
 *
 *  The art fonts in art/fonts.spe are CP437, which has no a-tilde and no
 *  o-tilde, so Portuguese cannot be written with them. This reads a font in
 *  the .hex format used by unscii and GNU Unifont and lays it out as the
 *  256-glyph atlas JCFont already knows how to cut up, so the renderer, the
 *  widgets and every width calculation in the engine stay exactly as they are.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_UI_HEXFONT_H_
#define ABUSE_UI_HEXFONT_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <map>

class image;

namespace abuse::ui {

// Which 256 codepoints the atlas is built from. The engine indexes glyphs by
// byte, and every width in it is a byte count, so the font is built for one
// single-byte encoding rather than made to understand UTF-8.
enum class Codepage
{
    // What the shipped translations and the art fonts use.
    CP437,
    // Byte value equals codepoint. Needed for Portuguese.
    Latin1
};

uint16_t const *codepage_table(Codepage cp);

class HexFont
{
public:
    // Reads through open_file, so the classic-data overlay applies.
    bool Load(char const *filename);

    bool Empty() const { return m_glyphs.empty(); }
    size_t Size() const { return m_glyphs.size(); }

    // Eight rows, most significant bit leftmost, or NULL when the font has no
    // glyph for this codepoint.
    uint8_t const *Find(uint32_t codepoint) const;

private:
    // The .hex format also carries 8x16 and 16x16 glyphs. Only the 8x8 ones
    // are kept: a font of mixed cell sizes has no single atlas layout.
    std::map<uint32_t, std::array<uint8_t, 8>> m_glyphs;
};

// A 256x64 image in the 32 by 8 layout JCFont expects, painted in `ink`.
// Codepoints the font does not have are left blank, which JCFont skips.
// The caller owns the image, and may delete it as soon as the JCFont is built.
image *build_atlas(HexFont const &font, Codepage cp, uint8_t ink);

// The ink colour the art fonts are painted in, so text keeps its colour when
// drawn without an explicit one.
extern uint8_t const kFontInk;

// Config `font=`, or -font on the command line.
bool parse_font_choice(char const *name, bool &extended);

// True when the extended font should replace the art font: either the player
// asked for it, or the language cannot be written without it.
bool extended_font_wanted();
void set_extended_font(bool on);

// Builds the atlas for the language in force, or NULL when the font file is
// missing, which leaves the classic art font in place.
image *build_extended_atlas();

}

#endif
