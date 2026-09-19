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
