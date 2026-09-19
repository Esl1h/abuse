/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See classic_data_screen.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include "classic_data_screen.h"

#include <stdio.h>
#include <string.h>

#include <string>

#include <SDL3/SDL.h>

#include "data/paths.h"
#include "i18n/uitext.h"
#include "jwindow.h"
#include "keys.h"
#include "menu_list.h"
#include "overlay.h"

extern WindowManager *wm;

namespace abuse::ui {

namespace {

using i18n::say;

enum Choice
{
    OpenPage,
    PlayAnyway,
    ChoiceCount
};

// Set once the player has asked for the page, so the screen can say what
// happened instead of looking like nothing did.
i18n::Phrase const *g_result = nullptr;

}

bool classic_data_missing()
{
    return data::mode() == data::Mode::Original && !data::classic_data_present();
}

void draw_classic_data_screen(int selected)
{
    // The two choices carry no value column: the address and the path are long
    // enough to dominate the panel width, and a wide panel is a small one,
    // because the text shrinks until it fits.
    Row rows[ChoiceCount];
    rows[OpenPage].label = say(i18n::kClassicOpen);
    rows[PlayAnyway].label = say(i18n::kClassicGoOn);

    // Held in a local: classic_data_dir returns by value, and the pointer into
    // a temporary would be dangling by the time the list is drawn.
    std::string where = data::classic_data_dir();

    char const *footers[6];
    int n = 0;
    footers[n++] = say(i18n::kClassicWhy1);
    footers[n++] = say(i18n::kClassicWhy2);
    footers[n++] = data::classic_data_url();
    footers[n++] = say(i18n::kClassicScript);
    footers[n++] = where.c_str();
    if (g_result)
        footers[n++] = say(*g_result);

    draw_list(say(i18n::kClassicTitle), rows, ChoiceCount, selected, footers, n);
}

void run_classic_data_screen()
{
    int selected = 0;
    bool quit = false;
    g_result = nullptr;

    while (!quit)
    {
        draw_classic_data_screen(selected);
        wm->flush_screen();

        Event ev;
        wm->get_event(ev);
        if (ev.type != EV_KEY)
            continue;

        switch (ev.key)
        {
        case JK_UP:
            selected = list_wrap(selected, -1, ChoiceCount);
            break;
        case JK_DOWN:
            selected = list_wrap(selected, 1, ChoiceCount);
            break;
        case JK_ENTER:
        case JK_SPACE:
            if (selected == OpenPage)
            {
                // The whole of the download feature. A browser is a thing every
                // desktop has, and this needs no library that the game would
                // then have to ship on three systems.
                if (SDL_OpenURL(data::classic_data_url()))
                    g_result = &i18n::kClassicOpened;
                else
                {
                    g_result = &i18n::kClassicNoOpen;
                    printf("Classic data: %s\n", SDL_GetError());
                }
            }
            else
                quit = true;
            break;
        case JK_ESC:
            quit = true;
            break;
        default:
            break;
        }
    }

    overlay().Clear();
    wm->flush_screen();
}

}
