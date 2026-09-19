/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See gamepad.h.
 *
 *  This software was released into the Public Domain.
 */

#include "gamepad.h"

#include <math.h>
#include <stddef.h>
#include "compat.h"

namespace abuse::input {

namespace {

PadState g_pad;
Deadzone g_deadzone;

// A trigger rests reliably at zero, so it needs only enough slack to ignore
// electrical noise. 8192, which a stick needs against drift, costs a quarter
// of the trigger travel before the shot registers.
Deadzone g_trigger_deadzone = { 2000, 30000 };

}

void PadState::reset()
{
    for (int i = 0; i < kMaxButtons; i++)
        m_buttons[i] = false;
    for (int i = 0; i < kMaxAxes; i++)
        m_axes[i] = 0;
}

void PadState::set_button(int button, bool down)
{
    if (button < 0 || button >= kMaxButtons)
        return;
    m_buttons[button] = down;
}

void PadState::set_axis(int axis, int value)
{
    if (axis < 0 || axis >= kMaxAxes)
        return;
    if (value < kAxisMin)
        value = kAxisMin;
    else if (value > kAxisMax)
        value = kAxisMax;
    m_axes[axis] = value;
}

bool PadState::button(int button) const
{
    if (button < 0 || button >= kMaxButtons)
        return false;
    return m_buttons[button];
}

int PadState::axis(int axis) const
{
    if (axis < 0 || axis >= kMaxAxes)
        return 0;
    return m_axes[axis];
}

float PadState::axis_scaled(int axis_index, Deadzone const &dz) const
{
    int v = axis(axis_index);
    int sign = v < 0 ? -1 : 1;
    int mag = v < 0 ? -v : v;

    // An outer smaller than the inner would invert the ramp; treat the pair as
    // unusable and fall back to raw magnitude rather than produce nonsense.
    int inner = dz.inner < 0 ? 0 : dz.inner;
    int outer = dz.outer > inner ? dz.outer : kAxisMax;

    if (mag <= inner)
        return 0.0f;
    if (mag >= outer)
        return (float)sign;

    // Rescaled so the value leaves the deadzone at 0 and not at a jump.
    float t = (float)(mag - inner) / (float)(outer - inner);
    return (float)sign * t;
}

bool PadState::axis_active(int axis_index, int sign, Deadzone const &dz) const
{
    if (sign == 0)
        return false;
    float v = axis_scaled(axis_index, dz);
    return sign < 0 ? v < 0.0f : v > 0.0f;
}

bool parse_deadzone_value(char const *text, int &out)
{
    if (!text || !*text)
        return false;

    // No strtol: a trailing character has to be a rejection, not a value that
    // happens to parse up to it. "80x0" is a typo, not 80.
    int value = 0;
    for (char const *c = text; *c; c++)
    {
        if (*c < '0' || *c > '9')
            return false;
        value = value * 10 + (*c - '0');
        if (value > kAxisMax)
            return false;       // no axis ever reaches that; an error, not saturation
    }

    out = value;
    return true;
}

namespace {

PadFamily g_family = PadFamily::Generic;

// Only the four face buttons and the shoulders differ between families; the
// d-pad and the sticks are called the same everywhere, so they fall through
// to the generic names.
struct FaceLabels { char const *south, *east, *west, *north; };

FaceLabels labels_for(PadFamily f)
{
    switch (f)
    {
    case PadFamily::Xbox:        return { "A", "B", "X", "Y" };
    case PadFamily::PlayStation: return { "Cross", "Circle", "Square", "Triangle" };
    // Nintendo swaps A/B and X/Y against Xbox, which is exactly the mix-up
    // this function exists to prevent.
    case PadFamily::Nintendo:    return { "B", "A", "Y", "X" };
    case PadFamily::Generic:     break;
    }
    return { "1", "2", "3", "4" };
}

}

char const *button_label(PadFamily family, int button)
{
    FaceLabels l = labels_for(family);
    switch (button)
    {
    case 0: return l.south;
    case 1: return l.east;
    case 2: return l.west;
    case 3: return l.north;
    case 9:  return "L";
    case 10: return "R";
    case 11: return "Up";
    case 12: return "Down";
    case 13: return "Left";
    case 14: return "Right";
    default: break;
    }
    return NULL;
}

char const *family_name(PadFamily f)
{
    switch (f)
    {
    case PadFamily::Xbox:        return "xbox";
    case PadFamily::PlayStation: return "playstation";
    case PadFamily::Nintendo:    return "nintendo";
    case PadFamily::Generic:     break;
    }
    return "generic";
}

bool parse_family(char const *name, PadFamily &out)
{
    if (!name)
        return false;
    if (strcasecmp(name, "xbox") == 0)
        out = PadFamily::Xbox;
    else if (strcasecmp(name, "playstation") == 0)
        out = PadFamily::PlayStation;
    else if (strcasecmp(name, "nintendo") == 0)
        out = PadFamily::Nintendo;
    else if (strcasecmp(name, "generic") == 0)
        out = PadFamily::Generic;
    else
        return false;
    return true;
}

PadFamily &pad_family()
{
    return g_family;
}

AimSettings &aim_settings()
{
    static AimSettings g_aim;
    return g_aim;
}

bool &pad_lost_flag()
{
    static bool g_lost = false;
    return g_lost;
}

bool pad_lost() { return pad_lost_flag(); }
void set_pad_lost(bool lost) { pad_lost_flag() = lost; }

CursorSettings &cursor_settings()
{
    static CursorSettings g_cursor;
    return g_cursor;
}

CursorStep menu_cursor_step(float x, float y, int pixels_per_tick)
{
    CursorStep step;
    if (pixels_per_tick < 1)
        return step;

    // Clamp to the circle, so a diagonal is not faster than a cardinal.
    float len = sqrtf(x * x + y * y);
    if (len > 1.0f)
    {
        x /= len;
        y /= len;
        len = 1.0f;
    }

    // Squared response: a small push nudges the cursor a pixel at a time,
    // which is what picking a save slot needs, and a full push still crosses
    // the screen quickly.
    float gain = len * len;
    if (len > 0.0f)
    {
        x = x / len * gain;
        y = y / len * gain;
    }

    step.x = (int)(x * (float)pixels_per_tick);
    step.y = (int)(y * (float)pixels_per_tick);

    // A deflection past the deadzone always moves at least one pixel;
    // rounding to zero would make the far end of the deadzone feel dead.
    if (step.x == 0 && x > 0.0f) step.x = 1;
    if (step.x == 0 && x < 0.0f) step.x = -1;
    if (step.y == 0 && y > 0.0f) step.y = 1;
    if (step.y == 0 && y < 0.0f) step.y = -1;

    return step;
}

PadState &pad_state()
{
    return g_pad;
}

Deadzone &trigger_deadzone()
{
    return g_trigger_deadzone;
}

Deadzone const &deadzone_for_axis(int axis_index)
{
    if (axis_index == kAxisLeftTrigger || axis_index == kAxisRightTrigger)
        return g_trigger_deadzone;
    return g_deadzone;
}

Deadzone &deadzone()
{
    return g_deadzone;
}

}
