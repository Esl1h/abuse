/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Native resolution overlay. Phase 4, task 4.1.
 *
 *  The game draws into a 320x200 buffer that is scaled up to the window, so
 *  anything drawn there is scaled with it: at 1080p a letter is a 5 by 8 block
 *  of fat pixels. This layer is the size of the window instead and is
 *  composited after that scaling, so its text stays sharp however large the
 *  window is.
 *
 *  It holds pixels and nothing else. What to draw, and how to navigate it, is
 *  elsewhere; keeping this free of SDL and of the game keeps it testable.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_UI_OVERLAY_H_
#define ABUSE_UI_OVERLAY_H_

#include <stdint.h>

#include <vector>

namespace abuse::ui {

class HexFont;

// Straight 0xAARRGGBB, the order SDL_PIXELFORMAT_ARGB8888 wants on a little
// endian machine, so a row can be handed to a texture without conversion.
using Colour = uint32_t;

constexpr Colour rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return ((Colour)a << 24) | ((Colour)r << 16) | ((Colour)g << 8) | b;
}

class Overlay
{
public:
    // Resizes when the window has changed and clears to fully transparent.
    // False when the size makes no sense, which leaves the buffer alone.
    bool Begin(int width, int height);

    void Clear();

    int Width() const { return m_width; }
    int Height() const { return m_height; }
    bool Empty() const { return m_pixels.empty(); }

    // Source over destination, so a half transparent panel shows the game
    // through it.
    void Blend(int x, int y, Colour c);

    void FillRect(int x, int y, int w, int h, Colour c);

    // One pixel thick, drawn inside the rectangle given.
    void FrameRect(int x, int y, int w, int h, Colour c);

    // Draws through `font` at `scale` times its natural 8 by 8 cell. Returns
    // the x the next character would start at, so callers can chain.
    int Text(HexFont const &font, int x, int y, char const *text, int scale,
             Colour c);

    // Width the same call would cover, for laying out before drawing.
    static int TextWidth(char const *text, int scale);
    static int TextHeight(int scale) { return 8 * scale; }

    uint32_t const *Pixels() const { return m_pixels.data(); }
    int Pitch() const { return m_width * (int)sizeof(uint32_t); }

    // True when anything has been drawn since the last Begin. A frame with an
    // untouched overlay skips the upload entirely.
    bool Dirty() const { return m_dirty; }

private:
    std::vector<uint32_t> m_pixels;
    int m_width = 0;
    int m_height = 0;
    bool m_dirty = false;
};

// The one the renderer composites. Drawing into it is what puts it on screen;
// it is cleared at the start of every frame that uses it.
Overlay &overlay();

// The glyphs the overlay draws with, loaded once from the font asset. Empty
// when the file is missing, and Text() then draws nothing rather than crash.
HexFont const &overlay_font();

}

#endif
