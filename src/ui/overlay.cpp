/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See overlay.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "overlay.h"

#include <string.h>

#include "hexfont.h"

namespace abuse::ui {

namespace {

// The overlay is Latin-1, like the rest of the text the engine handles: one
// byte is one glyph, so a width is a byte count.
uint32_t codepoint_of(unsigned char c)
{
    return (uint32_t)c;
}

}

bool Overlay::Begin(int width, int height)
{
    if (width < 1 || height < 1)
        return false;

    if (width != m_width || height != m_height)
    {
        m_pixels.assign((size_t)width * (size_t)height, 0);
        m_width = width;
        m_height = height;
    }
    else
        Clear();

    m_dirty = false;
    return true;
}

void Overlay::Clear()
{
    // The game clears the overlay every frame, including before anything has
    // ever sized it, and data() on an empty vector may be null.
    if (!m_pixels.empty())
        memset(m_pixels.data(), 0, m_pixels.size() * sizeof(uint32_t));
    m_dirty = false;
}

void Overlay::Blend(int x, int y, Colour c)
{
    if (x < 0 || y < 0 || x >= m_width || y >= m_height)
        return;

    uint32_t alpha = c >> 24;
    if (alpha == 0)
        return;

    m_dirty = true;
    uint32_t &dst = m_pixels[(size_t)y * (size_t)m_width + (size_t)x];

    if (alpha == 255)
    {
        dst = c;
        return;
    }

    uint32_t inv = 255 - alpha;
    uint32_t dr = (dst >> 16) & 0xff, dg = (dst >> 8) & 0xff, db = dst & 0xff;
    uint32_t da = (dst >> 24) & 0xff;
    uint32_t sr = (c >> 16) & 0xff, sg = (c >> 8) & 0xff, sb = c & 0xff;

    uint32_t r = (sr * alpha + dr * inv) / 255;
    uint32_t g = (sg * alpha + dg * inv) / 255;
    uint32_t b = (sb * alpha + db * inv) / 255;
    uint32_t a = alpha + da * inv / 255;

    dst = (a << 24) | (r << 16) | (g << 8) | b;
}

void Overlay::FillRect(int x, int y, int w, int h, Colour c)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            Blend(x + i, y + j, c);
}

void Overlay::FrameRect(int x, int y, int w, int h, Colour c)
{
    if (w < 1 || h < 1)
        return;

    for (int i = 0; i < w; i++)
    {
        Blend(x + i, y, c);
        Blend(x + i, y + h - 1, c);
    }
    for (int j = 1; j < h - 1; j++)
    {
        Blend(x, y + j, c);
        Blend(x + w - 1, y + j, c);
    }
}

int Overlay::Text(HexFont const &font, int x, int y, char const *text,
                  int scale, Colour c)
{
    if (!text || scale < 1)
        return x;

    for (unsigned char const *p = (unsigned char const *)text; *p; p++)
    {
        uint8_t const *rows = font.Find(codepoint_of(*p));
        if (rows)
        {
            for (int gy = 0; gy < 8; gy++)
                for (int gx = 0; gx < 8; gx++)
                {
                    if (!(rows[gy] & (0x80 >> gx)))
                        continue;
                    // One font pixel is a scale by scale block, which is what
                    // keeps the letterforms crisp instead of interpolated.
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++)
                            Blend(x + gx * scale + sx, y + gy * scale + sy, c);
                }
        }
        x += 8 * scale;
    }

    return x;
}

int Overlay::TextWidth(char const *text, int scale)
{
    if (!text || scale < 1)
        return 0;
    return (int)strlen(text) * 8 * scale;
}

Overlay &overlay()
{
    static Overlay g_overlay;
    return g_overlay;
}

HexFont const &overlay_font()
{
    static HexFont g_font;
    static bool tried = false;
    if (!tried)
    {
        tried = true;
        g_font.Load("fonts/unscii-8-thin.hex");
    }
    return g_font;
}

}
