/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  The start menu. Phase 4, task 4.4.
 *
 *  In place of the strip of icons down the right edge of the title screen:
 *  a list, in words, in the player's language, reachable with a d-pad and
 *  legible at 1080p. The icons remain one line of abuserc away
 *  (startmenu=classic), because every visual change in this project is
 *  optional.
 *
 *  It is also the menu reached with Esc from inside a level, which is why
 *  the first row is Resume when there is a level to go back to.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_UI_START_MENU_H_
#define ABUSE_UI_START_MENU_H_

namespace abuse::ui {

// What the player asked for. Everything else the menu does itself and keeps
// running: options, controls, credits, brightness, volume, difficulty.
enum class StartAction
{
    Resume,     // back to the level that is already loaded
    Play,       // start the first level
    Continue,   // the load-game screen picked a save
    Quit,       // and the confirmation was answered yes
    Idle,       // nobody touched it; the caller may run a demo
};

// Runs until one of the above. The caller acts on it: the menu knows what was
// asked for, not how the game starts a level.
StartAction run_start_menu();

// Draws one frame of it, for a scripted capture.
void draw_start_menu(int selected);

// The same, with the two things that depend on this host's disk pinned: the
// save to continue from and the difficulty the last player left behind. A
// golden frame has to look the same on every machine that records it.
void draw_start_menu_pinned(int selected);

// How many rows it has right now, which depends on whether a level is loaded
// and whether there is a save to continue from.
int start_menu_item_count();

// startmenu=classic in abuserc keeps the icons. Parsed in setup().
bool classic_start_menu();
void set_classic_start_menu(bool on);
bool parse_start_menu_choice(char const *name, bool &classic);

}

#endif
