/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Force feedback. Phase 3, task 3.3.
 *
 *  The decision of what a given event should feel like is kept here, away from
 *  SDL, so it can be unit tested. Actually shaking the pad is one call in
 *  src/sdlport, which is the only place that knows about SDL_RumbleGamepad.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_INPUT_RUMBLE_H_
#define ABUSE_INPUT_RUMBLE_H_

#include <stdint.h>

namespace abuse::input {

enum class RumbleEvent
{
    Shot,       // the player fired
    Hurt,       // the player took damage
    Explosion   // something blew up nearby
};

struct RumbleSettings
{
    // 0 disables force feedback entirely, which is what a player who finds it
    // distracting will reach for first.
    int strength = 100;         // percent, 0 to 100
};

struct RumbleCommand
{
    uint16_t low = 0;           // heavy motor, as SDL takes it
    uint16_t high = 0;          // light motor
    uint32_t duration_ms = 0;

    bool silent() const { return duration_ms == 0 || (low == 0 && high == 0); }
};

// What one event should feel like, at the configured strength. A shot is a
// short tick on the light motor; damage is a heavier, longer jolt; an
// explosion is the heaviest. Returns a silent command when strength is 0.
RumbleCommand rumble_for(RumbleEvent e, RumbleSettings const &s);

// Percent from the config. Rejects anything outside 0..100 so a typo keeps
// the default rather than shaking the pad for a second and a half.
bool parse_rumble_strength(char const *text, int &out);

RumbleSettings &rumble_settings();

}

#endif
