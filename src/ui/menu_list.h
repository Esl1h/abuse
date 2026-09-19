/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  The one panel shape the new screens use. Phase 4, task 4.1.
 *
 *  A title, rows of label on the left and value on the right, and footer lines
 *  under them. Both the options screen and the controls screen are this, and
 *  having the layout in one place is what keeps them from drifting apart.
 *
 *  The caller formats every string. This decides only where things go, which
 *  is why it needs no knowledge of what the rows mean.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_UI_MENU_LIST_H_
#define ABUSE_UI_MENU_LIST_H_

namespace abuse::ui {

struct Row
{
    char const *label = nullptr;
    char const *value = nullptr;
    // Draws a trailing asterisk on the label, for a row whose new value only
    // takes effect on the next run.
    bool marked = false;
};

// Everything the panel needs to know about its own size, worked out from the
// window and the longest string in it. Exposed so it can be tested without a
// window, and so a caller can ask where a row landed.
struct ListLayout
{
    int scale = 1;      // font cells are 8 by 8 times this
    int cell = 8;       // one glyph
    int row = 11;       // one row, glyph plus gap
    int pad = 8;

    int x = 0;          // panel
    int y = 0;
    int width = 0;
    int height = 0;

    int first_row_y = 0;
    int footer_y = 0;
};

// Widest row content and footer, in glyph cells. Separate from the layout so
// the caller can measure once and lay out at several scales.
int list_body_cells(Row const *rows, int count,
                    char const *const *footers, int footer_count);

// Picks the largest scale whose panel fits `width` by `height` and works out
// where everything goes. `body_cells` comes from list_body_cells.
ListLayout list_layout(int width, int height, int body_cells, int row_count,
                       int footer_count);

// Draws it centred in the window. `selected` may be -1 for no highlight.
void draw_list(char const *title, Row const *rows, int count, int selected,
               char const *const *footers, int footer_count);

// The same, centred in a rectangle of window pixels instead of the whole
// window. For a screen that shares the window with something else, such as
// the language screen sitting beside the cover art.
void draw_list_in(int rx, int ry, int rw, int rh,
                  char const *title, Row const *rows, int count, int selected,
                  char const *const *footers, int footer_count);

// The text size a panel of this height uses. Exposed for the notices, which
// are not lists but have to match their type size.
int list_scale_for(int height);

// Moves a selection by `delta`, wrapping at both ends, which is what a list
// navigated with a d-pad has to do.
int list_wrap(int selected, int delta, int count);

}

#endif
