/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Light as data, applied in RGB. Phase 6, block 6.2.
 *
 *  The 1995 lighting works on palette indices. `calc_light_table` builds a
 *  256 by 64 table: for each colour, sixty-four darker versions, each one
 *  channel-minus-one from the last, and every one of them snapped to the
 *  nearest entry in the 256-colour palette. Lighting a frame means replacing
 *  each pixel's index with the one that table names.
 *
 *  That snap is where the banding in every dark corner comes from, and it is
 *  also why a coloured light is impossible: there is no room in a 256-entry
 *  palette for the same scene in several tints.
 *
 *  This is the same darkening curve without the snap. The level per pixel is
 *  collected into a map while the frame is drawn, and applied to the real
 *  colour on the way to the window, where there are 24 bits to land in
 *  instead of 8.
 *
 *  The map is 320 by 200 bytes and the conversion already walks every pixel,
 *  so this costs a multiply-free subtraction per channel per pixel, on the
 *  CPU, and needs no GPU work at all.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_RENDER_LIGHTMAP_H_
#define ABUSE_RENDER_LIGHTMAP_H_

#include <stdint.h>

#include <vector>

namespace abuse::render {

// Full brightness. The engine's levels run 0 to 63, and 63 means untouched.
int const kLightLevels = 64;
int const kFullLight = kLightLevels - 1;

// One channel at a light level, the same curve the table bakes: one less per
// level below full, never past zero.
inline uint8_t shade_channel(uint8_t c, int level)
{
    if (level >= kFullLight)
        return c;
    int drop = kFullLight - (level < 0 ? 0 : level);
    return (uint8_t)(c > drop ? c - drop : 0);
}

// 0xAARRGGBB, opaque, which is what the window texture wants.
inline uint32_t shade(uint8_t r, uint8_t g, uint8_t b, int level)
{
    return 0xff000000u
           | ((uint32_t)shade_channel(r, level) << 16)
           | ((uint32_t)shade_channel(g, level) << 8)
           | (uint32_t)shade_channel(b, level);
}

// A light level per pixel of the game buffer. Written by the lighting pass,
// read by the conversion to the window.
class LightMap
{
public:
    void resize(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }
    bool empty() const { return m_levels.empty(); }

    // Everything at full brightness, which is what an unlit frame means.
    void clear();

    // Out of bounds writes are dropped and out of bounds reads answer full
    // brightness: the lighting pass works in clipped runs and rounding a run
    // up is cheaper than testing every pixel inside it.
    void fill(int x, int y, int run, int level);
    uint8_t at(int x, int y) const;

    uint8_t const *row(int y) const;

private:
    std::vector<uint8_t> m_levels;
    int m_width = 0;
    int m_height = 0;
};

// The one the lighting pass fills and the presentation reads.
LightMap &lightmap();

// Whether the frame is lit in RGB on the way out instead of by remapping
// palette indices. Off keeps the 1995 path exactly, which is what the
// Original mode and the classic preset use.
bool rgb_lighting();
void set_rgb_lighting(bool on);

}

#endif
