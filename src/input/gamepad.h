/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Gamepad state, kept as state rather than as events.
 *  Phase 3, task 3.3.
 *
 *  The upstream path turns pad events into synthetic key presses
 *  (src/sdlport/event.cpp). That works for the d-pad but is fragile on the
 *  analogue axes: it decides which direction to release from the sign of the
 *  value at the moment of release, and a stick that settles a hair past centre
 *  releases the direction the player was not holding, leaving the other one
 *  pressed. Holding the state and asking it each tick removes the whole class
 *  of problem.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_INPUT_GAMEPAD_H_
#define ABUSE_INPUT_GAMEPAD_H_

#include <stdint.h>

namespace abuse::input {

// Axis values arrive in SDL's range.
constexpr int kAxisMin = -32768;
constexpr int kAxisMax = 32767;

// SDL_GAMEPAD_AXIS_LEFT_TRIGGER and SDL_GAMEPAD_AXIS_RIGHT_TRIGGER. Spelled
// out as plain ints, not included from SDL_gamepad.h, because the rest of
// src/input stays SDL-agnostic; keep these in sync with SDL3's enum.
constexpr int kAxisLeftTrigger = 4;
constexpr int kAxisRightTrigger = 5;

struct Deadzone
{
    // Below `inner` the axis reads as centred: the stick never rests exactly
    // at zero and a worn one rests further away.
    int inner = 8192;       // 0x2000, the value the upstream event code uses
    // Above `outer` the axis reads as fully deflected, so a stick that cannot
    // physically reach the corner still gives full speed.
    int outer = 30000;
};

// Deadzone in [0, kAxisMax], parsed from abuserc or the command line. Invalid
// input (not a number, negative, or above kAxisMax, which no axis value can
// ever reach) is rejected so the caller can keep its current default instead
// of storing garbage, mirroring render::parse_scale_mode.
bool parse_deadzone_value(char const *s, int &out);

class PadState
{
public:
    static constexpr int kMaxButtons = 24;   // SDL_GAMEPAD_BUTTON_COUNT fits
    static constexpr int kMaxAxes = 8;

    void reset();

    void set_button(int button, bool down);
    void set_axis(int axis, int value);

    bool button(int button) const;
    int axis(int axis) const;

    // The axis as a fraction in [-1,1], with the deadzone applied. The value
    // is rescaled so it starts at 0 just past `inner` instead of jumping.
    float axis_scaled(int axis, Deadzone const &dz) const;

    // Whether the axis is deflected past the deadzone in the given direction.
    // `sign` is -1 or 1; this is what an axis binding asks.
    bool axis_active(int axis, int sign, Deadzone const &dz) const;

    // A pad that disconnects must not leave anything held down.
    void disconnect() { reset(); }

private:
    bool m_buttons[kMaxButtons] = {};
    int m_axes[kMaxAxes] = {};
};

// Which label to print for a pad button. The same physical button is called
// A on an Xbox pad, B on a Nintendo one, and Cross on a PlayStation one, and
// telling the player to "press A" on a DualSense is telling them to press the
// wrong button.
enum class PadFamily
{
    Generic,
    Xbox,
    PlayStation,
    Nintendo
};

// SDL's button index (SDL_GAMEPAD_BUTTON_SOUTH and friends) to the label that
// family prints on the plastic. Returns NULL for a button with no label.
char const *button_label(PadFamily family, int button);

char const *family_name(PadFamily f);
bool parse_family(char const *name, PadFamily &out);

PadFamily &pad_family();

// How far from the character the aim cursor can sit, in game pixels, when the
// right stick is pushed all the way. The upstream value worked out to 32,
// which on a 320 pixel wide screen keeps the crosshair almost on top of the
// player and makes distant targets unreachable.
struct AimSettings
{
    int radius = 80;
};

AimSettings &aim_settings();

// How far the menu cursor travels in one tick for a stick deflected to
// (x, y), each in [-1, 1] after the deadzone. Per tick and not per event:
// SDL only sends an axis event when the value changes, so a cursor driven by
// events stops dead whenever the stick is held steady.
struct CursorStep
{
    int x = 0;
    int y = 0;
};

CursorStep menu_cursor_step(float x, float y, int pixels_per_tick);

// Pixels per tick at full deflection. At 15 ticks a second this crosses the
// 320 pixel screen in about a second and a third.
struct CursorSettings
{
    int speed = 16;
};

CursorSettings &cursor_settings();

// True from the moment a pad is unplugged until one is plugged back in. The
// game pauses and says so rather than leaving the player standing still with
// a controller that no longer answers.
bool pad_lost();
void set_pad_lost(bool lost);

PadState &pad_state();
Deadzone &deadzone();

// Separate deadzone for the trigger axes. A trigger rests reliably at zero,
// unlike an analogue stick, so it can use a much smaller inner cutoff without
// the drift problems that make the stick need a wide one; sharing one
// Deadzone between both made the right trigger (fire) need a quarter of its
// travel before firing on hardware that reports it as an axis.
Deadzone &trigger_deadzone();

// Which Deadzone applies to a given SDL_GamepadAxis index. Kept here, next to
// PadState, rather than left for every caller to work out on its own: the
// axis-to-deadzone mapping is a single policy decision, and PadState's
// axis_scaled/axis_active still take an explicit Deadzone so they stay
// generic and easy to unit test with arbitrary values.
Deadzone const &deadzone_for_axis(int axis_index);

}

#endif
