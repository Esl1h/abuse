/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See language_screen.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include "language_screen.h"

#include <stdio.h>

#include "cache.h"
#include "data/config_file.h"
#include "game.h"
#include "hexfont.h"
#include "i18n/language.h"
#include "i18n/uitext.h"
#include "jwindow.h"
#include "keys.h"
#include "loader2.h"
#include "menu_list.h"
#include "options_screen.h"
#include "overlay.h"
#include "sdlport/setup.h"
#include "video.h"

extern WindowManager *wm;

namespace abuse::ui {

namespace {

using i18n::say;

// In the order they are offered. English first because it is the fallback,
// then the two translations the game shipped with, then ours. The pseudo
// language is deliberately absent: it is a development tool, and the options
// screen is where someone who wants it will look.
i18n::Language const kOffered[] = {
    i18n::Language::English,
    i18n::Language::French,
    i18n::Language::German,
    i18n::Language::Portuguese,
};

int const kOfferedCount = (int)(sizeof(kOffered) / sizeof(kOffered[0]));

// Each written in its own language, which is the only way a player who does
// not read the current one can find theirs.
char const *endonym(i18n::Language l)
{
    switch (l)
    {
    case i18n::Language::English:    return "English";
    case i18n::Language::French:     return "Fran\xe7" "ais";
    case i18n::Language::German:     return "Deutsch";
    case i18n::Language::Portuguese: return "Portugu\xeas do Brasil";
    case i18n::Language::Pseudo:     break;
    }
    return "?";
}

// The cover art the game already shows when a level is completed: 160 by 198
// down the left, with the message to its right. Reusing it keeps the first
// screen looking like the game instead of like a settings dialog, and adds no
// asset: it is public domain data already in the tree.
//
// Returns the width it took, in the game's own 320x200 pixels, so the caller
// can put the list in what is left.
int draw_backdrop_impl()
{
    main_screen->clear();

    int const im = cache.reg("art/frame.spe", "end_level_screen", SPEC_IMAGE, 1);
    image *art = im < 0 ? NULL : cache.img(im);
    if (art)
        main_screen->PutImage(art, ivec2(0, 0));

    main_screen->AddDirty(ivec2(0), main_screen->Size());
    return art ? art->Size().x : 0;
}

}

int draw_language_backdrop()
{
    return draw_backdrop_impl();
}

bool language_unchosen()
{
    return !language_was_configured();
}

void apply_language_change()
{
    load_language_table();
    if (the_game)
        the_game->build_fonts();
}

void draw_language_screen(int selected, int art_width)
{
    Row rows[kOfferedCount];
    for (int i = 0; i < kOfferedCount; i++)
    {
        rows[i].label = endonym(kOffered[i]);
        rows[i].value = i18n::language_name(kOffered[i]);
    }

    // The title carries both languages of this project and stays that way: it
    // is read by someone who has not chosen yet, so it cannot be written in
    // the language they are about to choose.
    char const *footers[2] = { "arrows and Enter", say(i18n::kMenuHint) };

    // Beside the art, never over it. The art is in the game's own 320x200
    // pixels and the overlay is in window pixels, so the edge has to be
    // converted rather than guessed at from a ratio.
    //
    // The region runs to the edge of the window, letterbox bar included:
    // that bar is empty, and the panel is what the player is reading.
    int ax, ay, aw, ah, win_w, win_h;
    if (art_width > 0
        && window_pixel_size(win_w, win_h)
        && game_rect_to_window(0, 0, art_width, 200, ax, ay, aw, ah)
        && ax + aw < win_w)
        draw_list_in(ax + aw, 0, win_w - (ax + aw), win_h,
                     "Language / Idioma", rows, kOfferedCount,
                     selected, footers, 2);
    else
        draw_list("Language / Idioma", rows, kOfferedCount, selected,
                  footers, 2);
}

void draw_language_screen(int selected)
{
    draw_language_screen(selected, 0);
}

void run_language_screen()
{
    int selected = 0;
    for (int i = 0; i < kOfferedCount; i++)
        if (kOffered[i] == i18n::language())
            selected = i;

    bool done = false;
    while (!done)
    {
        // Redrawn every pass: the options screen reached from here clears the
        // overlay on its way out, and a language change resizes this panel.
        int art_width = draw_backdrop_impl();
        draw_language_screen(selected, art_width);
        wm->flush_screen();

        Event ev;
        wm->get_event(ev);

        if (handle_global_key(ev))
            continue;

        if (ev.type != EV_KEY)
            continue;

        switch (ev.key)
        {
        case JK_UP:
            selected = list_wrap(selected, -1, kOfferedCount);
            break;
        case JK_DOWN:
            selected = list_wrap(selected, 1, kOfferedCount);
            break;
        case JK_LEFT:
        case JK_RIGHT:
            // The same keys the options screen uses, so a hand that learned
            // one works the other.
            selected = list_wrap(selected, ev.key == JK_RIGHT ? 1 : -1,
                                 kOfferedCount);
            break;
        case JK_ENTER:
        case JK_SPACE:
            done = true;
            break;
        default:
            break;
        }

        // Apply as the selection moves, so the list is read in the language
        // under the cursor and a wrong pick is obvious before it is confirmed.
        if (i18n::language() != kOffered[selected])
        {
            i18n::language() = kOffered[selected];
            apply_language_change();
        }
    }

    char const *path = config_file_path();
    if (!data::save_config_key(path, "language",
                               i18n::language_name(i18n::language())))
        printf("Language: %s (%s)\n", say(i18n::kNotSaved), path);

    overlay().Clear();
    main_screen->clear();
    main_screen->AddDirty(ivec2(0), main_screen->Size());
    wm->flush_screen();
}

}
