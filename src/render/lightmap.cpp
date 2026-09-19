/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See lightmap.h.
 *
 *  This software was released into the Public Domain.
 */

#include "lightmap.h"

namespace abuse::render {

namespace {
LightMap g_map;
bool g_rgb = false;
}

void LightMap::resize(int width, int height)
{
    if (width < 0) width = 0;
    if (height < 0) height = 0;

    if (width == m_width && height == m_height)
        return;

    m_width = width;
    m_height = height;
    m_levels.assign((size_t)width * (size_t)height, (uint8_t)kFullLight);
}

void LightMap::clear()
{
    for (size_t i = 0; i < m_levels.size(); i++)
        m_levels[i] = (uint8_t)kFullLight;
}

void LightMap::fill(int x, int y, int run, int level)
{
    if (m_levels.empty() || y < 0 || y >= m_height || run <= 0)
        return;

    if (x < 0)
    {
        run += x;
        x = 0;
    }
    if (x >= m_width || run <= 0)
        return;
    if (x + run > m_width)
        run = m_width - x;

    if (level < 0) level = 0;
    if (level > kFullLight) level = kFullLight;

    uint8_t *p = &m_levels[(size_t)y * (size_t)m_width + (size_t)x];
    for (int i = 0; i < run; i++)
        p[i] = (uint8_t)level;
}

uint8_t LightMap::at(int x, int y) const
{
    if (m_levels.empty() || x < 0 || y < 0 || x >= m_width || y >= m_height)
        return (uint8_t)kFullLight;
    return m_levels[(size_t)y * (size_t)m_width + (size_t)x];
}

uint8_t const *LightMap::row(int y) const
{
    if (m_levels.empty() || y < 0 || y >= m_height)
        return nullptr;
    return &m_levels[(size_t)y * (size_t)m_width];
}

void LightMap::smooth(int x, int y, int w, int h, int radius_x, int radius_y)
{
    if (m_levels.empty() || w <= 0 || h <= 0)
        return;

    // Clip the rectangle to the map, since the caller works in screen
    // coordinates that can start outside it.
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > m_width)  w = m_width - x;
    if (y + h > m_height) h = m_height - y;
    if (w <= 0 || h <= 0)
        return;

    if (radius_x < 0) radius_x = 0;
    if (radius_y < 0) radius_y = 0;
    if (radius_x == 0 && radius_y == 0)
        return;

    m_scratch.resize((size_t)w * (size_t)h);

    // Horizontal, into the scratch. A running sum, so the cost does not grow
    // with the radius.
    for (int row_i = 0; row_i < h; row_i++)
    {
        uint8_t const *src = &m_levels[(size_t)(y + row_i) * (size_t)m_width + (size_t)x];
        uint8_t *dst = &m_scratch[(size_t)row_i * (size_t)w];

        for (int i = 0; i < w; i++)
        {
            int lo = i - radius_x;
            int hi = i + radius_x;
            if (lo < 0) lo = 0;
            if (hi > w - 1) hi = w - 1;

            int sum = 0;
            for (int k = lo; k <= hi; k++)
                sum += src[k];
            dst[i] = (uint8_t)(sum / (hi - lo + 1));
        }
    }

    // Vertical, back into the map.
    for (int col = 0; col < w; col++)
    {
        for (int row_i = 0; row_i < h; row_i++)
        {
            int lo = row_i - radius_y;
            int hi = row_i + radius_y;
            if (lo < 0) lo = 0;
            if (hi > h - 1) hi = h - 1;

            int sum = 0;
            for (int k = lo; k <= hi; k++)
                sum += m_scratch[(size_t)k * (size_t)w + (size_t)col];

            m_levels[(size_t)(y + row_i) * (size_t)m_width + (size_t)(x + col)] =
                (uint8_t)(sum / (hi - lo + 1));
        }
    }
}

LightMap &lightmap()
{
    return g_map;
}

bool rgb_lighting()
{
    return g_rgb;
}

void set_rgb_lighting(bool on)
{
    g_rgb = on;
}

}
