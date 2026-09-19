/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See hexfont.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include "hexfont.h"

#include <stdlib.h>
#include <string.h>

#include <vector>

#include "i18n/language.h"
#include "image.h"
#include "specs.h"

namespace abuse::ui {

namespace {

// Unicode for each byte of CP437, which is what art/fonts.spe is laid out in
// and what french.lsp and german.lsp are written in.
uint16_t const kCP437[256] = {
#include "cp437.inc"
};

uint16_t g_latin1[256];

bool g_extended = false;
bool g_extended_forced_off = false;

int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}

uint8_t const kFontInk = 5;

uint16_t const *codepage_table(Codepage cp)
{
    if (cp == Codepage::CP437)
        return kCP437;

    if (!g_latin1[1])
        for (int i = 0; i < 256; i++)
            g_latin1[i] = (uint16_t)i;
    return g_latin1;
}

uint8_t const *HexFont::Find(uint32_t codepoint) const
{
    auto it = m_glyphs.find(codepoint);
    return it == m_glyphs.end() ? NULL : it->second.data();
}

bool HexFont::Load(char const *filename)
{
    bFILE *fp = open_file(filename, "rb");
    if (fp->open_failure())
    {
        delete fp;
        return false;
    }

    int size = fp->file_size();
    std::vector<char> buf(size + 1);
    int got = fp->read(buf.data(), size);
    delete fp;
    if (got != size)
        return false;
    buf[size] = 0;

    // "<codepoint in hex>:<two hex digits per row>", one glyph per line.
    char const *p = buf.data();
    while (*p)
    {
        char const *eol = strchr(p, '\n');
        char const *end = eol ? eol : p + strlen(p);

        char const *colon = (char const *)memchr(p, ':', end - p);
        if (colon)
        {
            uint32_t cp = 0;
            bool ok = colon > p;
            for (char const *c = p; c < colon && ok; c++)
            {
                int d = hex_digit(*c);
                if (d < 0)
                    ok = false;
                else
                    cp = cp * 16 + d;
            }

            // 16 digits is one byte per row over eight rows. Anything else is
            // a taller or wider glyph, which this atlas has no room for.
            if (ok && end - colon - 1 == 16)
            {
                std::array<uint8_t, 8> rows;
                for (int i = 0; i < 8 && ok; i++)
                {
                    int hi = hex_digit(colon[1 + i * 2]);
                    int lo = hex_digit(colon[2 + i * 2]);
                    if (hi < 0 || lo < 0)
                        ok = false;
                    else
                        rows[i] = (uint8_t)(hi * 16 + lo);
                }
                if (ok)
                    m_glyphs[cp] = rows;
            }
        }

        if (!eol)
            break;
        p = eol + 1;
    }

    return !m_glyphs.empty();
}

image *build_atlas(HexFont const &font, Codepage cp, uint8_t ink)
{
    uint16_t const *table = codepage_table(cp);

    // The same shape JCFont cuts the art fonts into: 32 glyphs across, 8 down.
    image *im = new image(ivec2(8 * 32, 8 * 8));
    im->clear(0);

    for (int c = 0; c < 256; c++)
    {
        uint8_t const *rows = font.Find(table[c]);
        if (!rows)
            continue;

        ivec2 origin((c % 32) * 8, (c / 32) * 8);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                if (rows[y] & (0x80 >> x))
                    im->PutPixel(origin + ivec2(x, y), ink);
    }

    return im;
}

bool parse_font_choice(char const *name, bool &extended)
{
    if (!name)
        return false;
    if (!strcasecmp(name, "classic"))
    {
        extended = false;
        return true;
    }
    if (!strcasecmp(name, "extended"))
    {
        extended = true;
        return true;
    }
    return false;
}

void set_extended_font(bool on)
{
    g_extended = on;
    g_extended_forced_off = !on;
}

bool extended_font_wanted()
{
    if (g_extended)
        return true;
    // Portuguese cannot be written in CP437, so choosing it chooses the font
    // too, unless the player has explicitly asked for the classic one.
    return !g_extended_forced_off
        && i18n::language_encoding(i18n::language()) == i18n::Encoding::Latin1;
}

image *build_extended_atlas()
{
    if (!extended_font_wanted())
        return NULL;

    HexFont font;
    if (!font.Load("fonts/unscii-8-thin.hex"))
    {
        printf("Font: fonts/unscii-8-thin.hex not found, keeping the classic font\n");
        return NULL;
    }

    Codepage cp = i18n::language_encoding(i18n::language()) == i18n::Encoding::Latin1
                    ? Codepage::Latin1 : Codepage::CP437;
    return build_atlas(font, cp, kFontInk);
}

}
