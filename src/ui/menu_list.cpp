/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See menu_list.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include "menu_list.h"

#include <stdio.h>
#include <string.h>

#include "hexfont.h"
#include "overlay.h"
#include "video.h"

namespace abuse::ui {

namespace {

// Deliberately plain. The visual theme is still open, and guessing at one here
// would only have to be undone.
Colour const kPanel   = rgba(12, 12, 20, 235);
Colour const kBorder  = rgba(150, 150, 170);
Colour const kInk     = rgba(210, 210, 220);
Colour const kDim     = rgba(130, 130, 145);
Colour const kPick    = rgba(255, 220, 120);
Colour const kPickBar = rgba(60, 60, 90, 200);

int cells(char const *s)
{
    return s ? (int)strlen(s) : 0;
}

// Rows the panel spends on everything that is not a list row, on top of the
// footer lines themselves: the title, a blank line under it, the half row
// before the footers, and a margin at the bottom.
int const kChromeRows = 4;

}

// One notch of text size per 480 pixels of height, which keeps the panel the
// same share of the window from 480p up.
int list_scale_for(int height)
{
    int s = height / 480;
    if (s < 2) s = 2;
    if (s > 6) s = 6;
    return s;
}

int list_body_cells(Row const *rows, int count,
                    char const *const *footers, int footer_count)
{
    int label_cells = 0, value_cells = 0, footer_cells = 0;

    for (int i = 0; i < count; i++)
    {
        int n = cells(rows[i].label) + (rows[i].marked ? 2 : 0);
        if (n > label_cells)
            label_cells = n;

        n = cells(rows[i].value);
        if (n > value_cells)
            value_cells = n;
    }

    for (int i = 0; i < footer_count; i++)
    {
        int n = cells(footers[i]);
        if (n > footer_cells)
            footer_cells = n;
    }

    // Three cells of air between the longest label and the value column.
    int body = label_cells + 3 + value_cells;
    return footer_cells > body ? footer_cells : body;
}

ListLayout list_layout(int width, int height, int body_cells, int row_count,
                       int footer_count)
{
    ListLayout l;

    int const rows = row_count + kChromeRows + footer_count;

    l.scale = list_scale_for(height);
    while (l.scale > 1
           && ((body_cells + 4) * 8 * l.scale > width
               || rows * (8 * l.scale + l.scale * 3) > height))
        l.scale--;

    l.cell = 8 * l.scale;
    l.row = l.cell + l.scale * 3;
    l.pad = l.cell;

    l.width = body_cells * l.cell + l.pad * 2;
    if (l.width > width)
        l.width = width;
    l.height = l.row * rows;
    if (l.height > height)
        l.height = height;

    l.x = (width - l.width) / 2;
    l.y = (height - l.height) / 2;
    if (l.y < 0)
        l.y = 0;

    // Title on the first row, a blank one after it.
    l.first_row_y = l.y + l.pad / 2 + l.row * 2;
    l.footer_y = l.first_row_y + l.row * row_count + l.row / 2;

    return l;
}

void draw_list(char const *title, Row const *rows, int count, int selected,
               char const *const *footers, int footer_count)
{
    int w = 0, h = 0;
    if (!window_pixel_size(w, h))
        return;
    draw_list_in(0, 0, w, h, title, rows, count, selected, footers,
                 footer_count);
}

void draw_list_in(int rx, int ry, int rw, int rh,
                  char const *title, Row const *rows, int count, int selected,
                  char const *const *footers, int footer_count)
{
    int w = 0, h = 0;
    if (!window_pixel_size(w, h))
        return;

    Overlay &ov = overlay();
    if (!ov.Begin(w, h))
        return;

    HexFont const &font = overlay_font();
    if (font.Empty())
        return;

    if (rw < 1 || rh < 1)
        return;

    int body = list_body_cells(rows, count, footers, footer_count);
    ListLayout l = list_layout(rw, rh, body, count, footer_count);
    l.x += rx;
    l.y += ry;
    l.first_row_y += ry;
    l.footer_y += ry;

    ov.FillRect(l.x, l.y, l.width, l.height, kPanel);
    ov.FrameRect(l.x, l.y, l.width, l.height, kBorder);

    int x = l.x + l.pad;

    if (title)
        ov.Text(font, x, l.y + l.pad / 2, title, l.scale, kPick);

    int y = l.first_row_y;
    for (int i = 0; i < count; i++)
    {
        bool on = i == selected;
        if (on)
            ov.FillRect(l.x + l.scale, y - l.scale,
                        l.width - l.scale * 2, l.cell + l.scale * 2, kPickBar);

        char label[160];
        snprintf(label, sizeof(label), "%s%s",
                 rows[i].label ? rows[i].label : "",
                 rows[i].marked ? " *" : "");
        ov.Text(font, x, y, label, l.scale, on ? kPick : kInk);

        if (rows[i].value)
        {
            int vx = l.x + l.width - l.pad
                     - Overlay::TextWidth(rows[i].value, l.scale);
            ov.Text(font, vx, y, rows[i].value, l.scale, on ? kPick : kDim);
        }

        y += l.row;
    }

    y = l.footer_y;
    for (int i = 0; i < footer_count; i++)
    {
        ov.Text(font, x, y, footers[i], l.scale, kDim);
        y += l.row;
    }
}

int list_wrap(int selected, int delta, int count)
{
    if (count < 1)
        return 0;
    int v = selected + delta;
    return ((v % count) + count) % count;
}

}
