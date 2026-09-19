/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Aim held as a vector from the player, not as a screen position.
 *  Phase 3, task 3.3.
 *
 *  The upstream path turns right-stick motion into synthetic mouse movement,
 *  so the crosshair is an absolute screen position that only moves when an
 *  axis event arrives. It therefore stays where it is while the player walks,
 *  and depends on event timing for its accuracy. Keeping an offset instead,
 *  recomputed every tick, makes the crosshair orbit the player on its own and
 *  removes the timing dependency.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_INPUT_AIM_H_
#define ABUSE_INPUT_AIM_H_

namespace abuse::input {

struct AimVector
{
    int x = 0;
    int y = 0;
};

// Offset of the crosshair from the player, in game pixels.
AimVector &aim_vector();

// True while the pad is the thing steering the aim. The mouse takes it back
// by moving, so a player with a pad plugged in can still use the mouse.
bool pad_aim_active();
void set_pad_aim_active(bool active);

// Recomputes the vector from the right stick, and answers whether the stick is
// deflected right now. A centred stick leaves the vector alone, which is the
// persistent aim the phase asks for.
bool update_aim_from_pad();

// Turns a stick reading into an offset, kept separate from the device so it
// can be unit tested. `radius` is the reach at full deflection.
AimVector aim_offset_from_axes(float x, float y, int radius);

// A candidate to aim at, as an offset from the player in game pixels.
struct AimTarget
{
    int dx = 0;
    int dy = 0;
};

struct AssistSettings
{
    // Percent. 0 disables it, and that is the default: helping the player aim
    // changes how the game plays, so it is opt-in. Always 0 in Original mode.
    int strength = 0;
    // How far off the current aim a candidate may be before it is ignored,
    // in degrees. Without a cone the aim would snap across the screen.
    int cone_degrees = 25;
};

AssistSettings &assist_settings();

// Biases `aim` towards the candidate closest to where the player is already
// pointing, keeping the length of `aim` so the reach does not change. Returns
// `aim` unchanged when assistance is off, when there are no candidates, or
// when none falls inside the cone.
AimVector assist_aim(AimVector const &aim, AimTarget const *targets, int count,
                     AssistSettings const &settings);

}

#endif
