/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  The Remastered HUD. Phase 4, task 4.6.
 *
 *  The classic status bar is art: a 320x200 strip with the health digits and
 *  the weapon icons baked into `art/statbar.spe`, magnified along with the
 *  game when the window is larger. This one is drawn into the native
 *  resolution overlay instead, so it stays sharp, and it has room for things
 *  the strip has no cells for.
 *
 *  It is what the Remastered mode draws. `hud=classic` in abuserc asks for
 *  the strip instead, and the Original mode always keeps the strip: that mode
 *  is the reference the snapshots compare against.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_UI_HUD_H_
#define ABUSE_UI_HUD_H_

namespace abuse::ui {

// True while the classic strip is the one to draw. The status bar asks this
// before drawing itself, which is what keeps the two from overlapping.
bool classic_hud();

// What the config says, ignoring the Original mode's override. The options
// row shows this: it is what the player set, and what gets written back.
bool classic_hud_configured();

void set_classic_hud(bool on);
bool parse_hud_choice(char const *name, bool &classic);

// Draws it into the overlay, once per frame, from Game::draw. Does nothing
// when the classic strip is on, when there is no level, or when the player
// object is gone.
void draw_hud();

// The same with the damage animation settled, for a scripted capture: a
// golden frame cannot depend on how long ago the player was hit.
void draw_hud_pinned();

}

#endif
