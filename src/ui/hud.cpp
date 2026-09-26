/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See hud.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include "hud.h"

#include <stdio.h>
#include <string.h>

#include "cache.h"
#include "compat.h"
#include "chars.h"
#include "clisp.h"
#include "data/paths.h"
#include "dev.h"
#include "extend.h"
#include "game.h"
#include "hexfont.h"
#include "lisp.h"
#include "menu_list.h"
#include "objects.h"
#include "sbar.h"
#include "overlay.h"
#include "timing.h"
#include "video.h"
#include "view.h"

extern palette *pal;

namespace abuse::ui {

namespace {

// The same restraint as the menus: no theme until task 4.1 settles one.
Colour const kInk      = rgba(210, 210, 220);
Colour const kDim      = rgba(130, 130, 145);
Colour const kWell     = rgba(10, 10, 16, 190);
Colour const kEdge     = rgba(150, 150, 170, 170);
Colour const kHealth   = rgba(205, 60, 45);
Colour const kHealthLo = rgba(230, 170, 40);
Colour const kGhost    = rgba(250, 240, 210, 170);
Colour const kPick     = rgba(255, 220, 120);

// An empty weapon slot: present enough to be counted, faint enough not to
// be mistaken for something being carried.

// The Remastered default since 2026-09-25. The Original mode is unaffected:
// classic_hud() forces the 1995 strip there whatever this says, because that
// strip is part of the picture the snapshots compare.
bool g_classic = false;
bool g_pinned = false;

// The weapon icons, the same ones the strip draws, registered once. The strip
// keeps its own copies; the cache hands both the same entry.
int g_weapon_icon[TOTAL_WEAPONS];
bool g_icons_ready = false;

void load_icons()
{
    if (g_icons_ready)
        return;

    char file[100];
    void *name = LSymbol::FindOrCreate("sbar_file");
    if (symbol_value(name) != l_undefined)
        strcpy(file, lstring_value(symbol_value(name)));
    else
        strcpy(file, "art/statbar.spe");

    for (int i = 0; i < TOTAL_WEAPONS; i++)
    {
        char entry[20];
        snprintf(entry, sizeof(entry), "bweap%04d.pcx", i + 1);
        g_weapon_icon[i] = cache.reg(file, entry, SPEC_IMAGE);
    }
    g_icons_ready = true;
}

// One 8-bit image into the overlay, `k` window pixels per game pixel. Colour
// zero is the transparent one throughout Abuse's art.
void blit(image *im, int x, int y, int k)
{
    Overlay &ov = overlay();
    ivec2 size = im->Size();

    for (int iy = 0; iy < size.y; iy++)
        for (int ix = 0; ix < size.x; ix++)
        {
            uint8_t c = im->Pixel(ivec2(ix, iy));
            if (!c)
                continue;

            Colour rgb = rgba((uint8_t)pal->red(c), (uint8_t)pal->green(c),
                              (uint8_t)pal->blue(c));
            ov.FillRect(x + ix * k, y + iy * k, k, k, rgb);
        }
}

// The bar the player watches: the current value in a solid block, and what
// was just lost still showing behind it for a moment. Purely cosmetic, so it
// runs on the wall clock rather than on ticks.
struct Drain
{
    int shown = -1;         // where the pale tail is
    int last = -1;          // the value it is chasing
    time_marker changed;

    int tail(int value)
    {
        if (last != value)
        {
            if (value > shown || shown < 0)
                shown = value;      // healing: no tail to leave behind
            last = value;
            changed.get_time();
        }

        if (g_pinned)
            return value;

        time_marker now;
        double held = now.diff_time(&changed);
        if (held > 0.4)
        {
            // 60 units a second once it starts moving, so a big hit takes
            // about as long to drain as a small one takes to notice.
            int drop = (int)((held - 0.4) * 60.0);
            if (shown - drop <= value)
                shown = value;
            else
                shown -= drop;
        }
        return shown;
    }
};

Drain g_health;

void draw_bar(int x, int y, int w, int h, int value, int tail, int full,
              Colour fill)
{
    Overlay &ov = overlay();
    ov.FillRect(x, y, w, h, kWell);

    if (full > 0)
    {
        int tw = tail > value ? (w - 2) * tail / full : 0;
        int vw = (w - 2) * value / full;
        if (tw > w - 2) tw = w - 2;
        if (vw > w - 2) vw = w - 2;
        if (tw > vw)
            ov.FillRect(x + 1 + vw, y + 1, tw - vw, h - 2, kGhost);
        if (vw > 0)
            ov.FillRect(x + 1, y + 1, vw, h - 2, fill);
    }

    ov.FrameRect(x, y, w, h, kEdge);
}

// What the player is carrying, drawn as the strip's own icons with the count
// under each one. The current weapon is the bright one.
void draw_weapons(view *v, int right, int bottom, int k, int text)
{
    load_icons();

    int n = total_weapons < TOTAL_WEAPONS ? total_weapons : TOTAL_WEAPONS;
    int gap = 2 * k;

    // Only what is being carried, packed against the right edge, so the row
    // grows as weapons are found.
    //
    // It used to draw every slot, empty ones included, the way the 1995
    // strip does: the gaps told a player there were more of these
    // somewhere. That needed a box per slot to be legible at all, and the
    // boxes were the first thing a player asked to have removed. Without
    // them an empty slot is an invisible hole, and one icon floating in the
    // middle of the screen says less than no gaps at all.
    //
    // Right to left, so the row grows away from the edge it is pinned to.
    int x = right;
    for (int i = n - 1; i >= 0; i--)
    {
        if (g_weapon_icon[i] < 0)
            continue;

        bool const owned = v->has_weapon(i) != 0;
        if (!owned)
            continue;

        image *im = cache.img(g_weapon_icon[i]);
        if (!im)
            continue;

        int iw = im->Size().x * k;
        int ih = im->Size().y * k;
        x -= iw;

        bool const current = v->current_weapon == i;
        int top = bottom - Overlay::TextHeight(text) - ih - k;

        // No box around the slot. It was there to group the icon with its
        // count and it only added furniture: the icons are already spaced
        // apart, and a grid of rectangles over the game reads as a menu
        // rather than as a read-out. Which weapon is in hand is said by the
        // bar under it and by the colour of the count.
        if (current)
            overlay().FillRect(x, bottom + k, iw, k, kPick);

        {
            char count[16];
            snprintf(count, sizeof(count), "%d", v->weapon_total(i));
            int cw = Overlay::TextWidth(count, text);

            blit(im, x, top, k);
            overlay().Text(overlay_font(), x + (iw - cw) / 2,
                           bottom - Overlay::TextHeight(text) - k, count, text,
                           current ? kPick : kDim);
        }
        x -= gap;
    }
}

}

bool classic_hud()
{
    // The Original mode is what the tests compare against, and its status bar
    // is part of the picture they compare.
    return g_classic || data::mode() == data::Mode::Original;
}

bool classic_hud_configured()
{
    return g_classic;
}

void set_classic_hud(bool on)
{
    g_classic = on;
}

bool parse_hud_choice(char const *name, bool &classic)
{
    if (!name)
        return false;
    if (strcasecmp(name, "classic") == 0)
    {
        classic = true;
        return true;
    }
    if (strcasecmp(name, "modern") == 0)
    {
        classic = false;
        return true;
    }
    return false;
}

void draw_hud()
{
    if (classic_hud() || !the_game || !current_level || !total_weapons)
        return;

    view *v = the_game->first_view;
    if (!v || !v->m_focus)
        return;

    int win_w = 0, win_h = 0;
    if (!window_pixel_size(win_w, win_h))
        return;

    Overlay &ov = overlay();
    if (!ov.Begin(win_w, win_h))
        return;
    if (overlay_font().Empty())
        return;

    // Inside the picture, not inside the window. The overlay covers the whole
    // window, letterbox bars included, and a HUD pinned to the window corners
    // ends up outside the game in fullscreen, which is where it was.
    //
    // The view rather than the whole 320x200 buffer: the status bar area at
    // the bottom is not part of what the player is looking at.
    int gx = 0, gy = 0, gw = win_w, gh = win_h;
    if (!game_rect_to_window(v->m_aa.x, v->m_aa.y,
                             v->m_bb.x - v->m_aa.x + 1,
                             v->m_bb.y - v->m_aa.y + 1, gx, gy, gw, gh))
    {
        gx = 0; gy = 0; gw = win_w; gh = win_h;
    }

    // Sized against the picture too, so the HUD keeps its proportion to the
    // game whatever shape the window is.
    int const text = list_scale_for(gh * 2);
    int const k = text;                 // one game pixel per text notch
    int const margin = 6 * k;

    int const w = gx + gw;              // right edge of the picture
    int const h = gy + gh;              // and its bottom

    // Health, bottom left. The number is the same one the strip shows, so a
    // player who knows the game reads it without learning anything.
    int hp = v->m_focus->hp();
    int full = 100;
    if (hp > full)
        full = hp;

    // At the same size as the rest of the overlay. It was drawn at twice
    // that, which made the health the loudest thing on screen when it is
    // the one number a player checks in passing.
    int const health_text = text;

    int bar_w = 28 * k;
    int bar_h = 4 * k;
    int bar_x = gx + margin;
    // Centred on the number beside it rather than on the bottom margin.
    int bar_y = h - margin - Overlay::TextHeight(health_text)
                + (Overlay::TextHeight(health_text) - bar_h) / 2;

    char number[16];
    snprintf(number, sizeof(number), "%d", hp);
    int num_w = Overlay::TextWidth("100", health_text);

    // The number first and the bar beside it, both on the same baseline: the
    // eye goes to the number, and the bar says how far from empty it is
    // without having to be read.
    ov.Text(overlay_font(), bar_x, h - margin - Overlay::TextHeight(health_text),
            number, health_text, hp * 4 < full ? kHealthLo : kInk);

    draw_bar(bar_x + num_w + 2 * k, bar_y, bar_w, bar_h, hp,
             g_health.tail(hp), full, hp * 4 < full ? kHealthLo : kHealth);

    draw_weapons(v, w - margin, h - margin, k, text);

    // The same switch the classic counter uses, in the dev menu.
    if (fps_on)
    {
        char fps[24];
        float ms = frame_ms();
        snprintf(fps, sizeof(fps), "%.1f fps", ms > 0.f ? 1000.f / ms : 0.f);
        ov.Text(overlay_font(), w - margin - Overlay::TextWidth(fps, text),
                gy + margin, fps, text, kDim);
    }
}

void draw_hud_pinned()
{
    g_pinned = true;
    draw_hud();
    g_pinned = false;
}

}
