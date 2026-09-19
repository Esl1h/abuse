/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Action map: names for what the player does, and the bindings that trigger
 *  them. Phase 3, task 3.1.
 *
 *  The per-tick input packet is the contract this has to honour. It carries
 *  eight flag bits and an aim position (src/view.cpp, SCMD_SET_INPUT), and the
 *  replays are recordings of those packets. Anything that produces the same
 *  bits leaves every recorded replay reproducible, whatever the player pressed
 *  to get there.
 *
 *  Everything outside those eight bits travels as raw key codes
 *  (SCMD_KEYPRESS), which is why a gamepad button has to resolve to a key
 *  code to reach anything else.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_INPUT_ACTIONS_H_
#define ABUSE_INPUT_ACTIONS_H_

#include <stdint.h>

namespace abuse::input {

// The actions that live in the input packet. The order matches nothing on the
// wire on purpose: to_flags() spells out the bit for each one, so renaming or
// reordering here can never silently change the protocol.
enum class Action
{
    Left,
    Right,
    Up,         // also "look up"; the game decides from context
    Down,       // also crouch
    Fire,
    Special,    // special weapon
    WeaponPrev,
    WeaponNext,

    Count
};

char const *action_name(Action a);       // "fire", "weapprev", ... as in abuserc
bool parse_action(char const *name, Action &out);

// Where a binding comes from. One action can have several, which is the point:
// the upstream config allows exactly one key per action, plus a hardcoded
// second key for the four directions.
enum class Source
{
    None,
    Key,            // key code from src/imlib/keys.h
    MouseButton,    // 1 left, 2 middle, 3 right
    PadButton,      // SDL_GamepadButton
    PadAxis         // SDL_GamepadAxis, with a sign
};

struct Binding
{
    Source source = Source::None;
    int code = 0;
    int sign = 0;   // PadAxis only: which half of the axis triggers

    bool empty() const { return source == Source::None; }
};

// Eight. The directions alone reach four with the stock defaults: two keys,
// a d-pad button and a stick axis. Four was the first guess and the defaults
// silently overflowed it, which is why add() reports failure and the caller
// now checks.
constexpr int kMaxBindings = 8;

class ActionMap
{
public:
    ActionMap();

    void clear(Action a);
    // Returns false when the action already has kMaxBindings, rather than
    // dropping the new one quietly.
    bool add(Action a, Binding b);
    int binding_count(Action a) const;
    Binding const &binding(Action a, int index) const;

    // True when some binding of the action matches, for a caller that knows
    // how to read the device state.
    bool has(Action a, Source source, int code) const;

    // Bindings the upstream config can express, so a config written by an
    // older build keeps working.
    void set_legacy_defaults();

private:
    Binding m_bindings[(int)Action::Count][kMaxBindings];
};

// Packs the pressed actions into the eight bits of SCMD_SET_INPUT. This is the
// only place that knows the wire layout.
uint8_t to_flags(bool const pressed[(int)Action::Count]);

// Asked whether one binding is currently active. Keeps device polling out of
// this module, which is what makes the resolution testable without a keyboard.
using BindingProbe = bool (*)(Binding const &binding, void *user);

// Fills `pressed`: an action is pressed when any of its bindings is active.
void resolve(ActionMap const &map, BindingProbe probe, void *user,
             bool pressed[(int)Action::Count]);

// One line of the `bind=` form in abuserc:
//
//     bind=fire,key,Space
//     bind=fire,pad,righttrigger+
//     bind=left,pad,dpleft
//     bind=fire,mouse,1
//
// Returns false on anything malformed, leaving both outputs untouched, so a
// typo drops one line instead of the whole config. Device names are resolved
// by the caller through `resolver`, which keeps SDL out of this module.
using PadNameResolver = bool (*)(char const *name, Source &source, int &code, int &sign);

bool parse_bind_line(char const *text, PadNameResolver resolver,
                     Action &action, Binding &binding);

// Renders a binding back into the `bind=` form, for writing a config out.
// `writer` turns a pad code back into its name. Returns false when the
// binding cannot be written, rather than emitting something unreadable.
using PadNameWriter = bool (*)(Source source, int code, int sign,
                               char *out, int out_size);

bool format_bind_line(Action action, Binding const &b, PadNameWriter writer,
                      char *out, int out_size);

// Named keyboard layouts. `classic` is what the game has always shipped, down
// to leaving the special weapon unbound, so an existing install is untouched
// unless the player asks for something else.
enum class KeyPreset
{
    Classic,
    Modern      // Q/E change weapon, F is the special, hands stay on WASD
};

bool parse_key_preset(char const *name, KeyPreset &out);
char const *key_preset_name(KeyPreset p);

// Fills the map with one of the layouts. Pad bindings are not touched: the
// preset is about the keyboard.
void apply_key_preset(KeyPreset p, ActionMap &map);

}

#endif
