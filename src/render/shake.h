/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Screen shake. Phase 6, block 6.4.
 *
 *  A knock to the camera when the player is hit. It moves where the frame is
 *  drawn from and nothing else: no object moves, no tick is affected, and a
 *  replay recorded with it on hashes the same as one recorded with it off.
 *
 *  Deterministic on purpose. The direction comes from a counter of its own
 *  and not from the game's RNG, because reaching into that would change
 *  every roll the simulation makes afterwards, which is exactly the kind of
 *  thing the replay tests exist to catch.
 *
 *  Free of SDL, so the decay curve can be tested without a window.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_RENDER_SHAKE_H_
#define ABUSE_RENDER_SHAKE_H_

namespace abuse::render {

// Adds a knock. `amount` is in game pixels of initial displacement; the
// callers pass something proportional to the damage taken. Knocks add up,
// with a ceiling, so a burst does not launch the camera off the level.
void shake(float amount);

// Decays it. Called once per logical tick, so the shake lasts the same time
// on every machine whatever the frame rate.
void shake_tick();

// Where the camera should be this frame, in game pixels. Zero when nothing
// is shaking, which is almost always.
void shake_offset(int &dx, int &dy);

// The amplitude left, for the tests.
float shake_amount();

// After a level load, a menu, or anything else that should not inherit the
// last frame's knock.
void reset_shake();

// The setting. Off means shake() does nothing at all.
bool shake_enabled();
void set_shake_enabled(bool on);

}

#endif
