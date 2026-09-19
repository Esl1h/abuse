/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  The language the player is asked for once, before anything else.
 *  Phase 4, task 4.3.
 *
 *  Asked before the intro, because the intro is the first thing with words in
 *  it. Asked only when nothing has said what the language is: once the choice
 *  is written to abuserc, the screen never appears again, and the options
 *  screen is where it gets changed afterwards.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_UI_LANGUAGE_SCREEN_H_
#define ABUSE_UI_LANGUAGE_SCREEN_H_

namespace abuse::ui {

// True when nobody has chosen yet: no `language=` in abuserc and no
// -language on the command line.
bool language_unchosen();

// Runs until the player picks one. Applies the choice immediately, reloading
// the symbol table and the font, and writes it to abuserc.
void run_language_screen();

// Draws one frame of it, for a scripted capture. The second form places the
// list beside art of the given width, in the game's own 320 pixel units.
void draw_language_screen(int selected);
void draw_language_screen(int selected, int art_width);

// Paints the cover art the screen sits beside, into the game's own buffer,
// and returns its width in the game's 320 pixel units. Exposed so a scripted
// capture can show the whole screen rather than the panel alone.
int draw_language_backdrop();

// Applies a language the player just picked: reloads the Lisp symbol table
// and rebuilds the fonts, so it takes effect without a restart. Also used by
// the options screen.
void apply_language_change();

}

#endif
