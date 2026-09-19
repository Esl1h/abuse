/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include <ctype.h>

#include "common.h"

#include "sdlport/joy.h"
#include "game.h"

#include "keys.h"
#include "lisp.h"
#include "jwindow.h"
#include "configuration.h"
#include "harness.h"
#include "input/actions.h"
#include "input/gamepad.h"

#include <SDL3/SDL_gamepad.h>

// src/input stays free of SDL headers, so it spells the trigger axis indices
// as plain ints. This is where the two meet, so this is where the promise to
// keep them in sync becomes something the compiler checks.
static_assert(abuse::input::kAxisLeftTrigger == SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
              "trigger axis index drifted from SDL");
static_assert(abuse::input::kAxisRightTrigger == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
              "trigger axis index drifted from SDL");

extern int get_key_binding(char const *dir, int i);

int key_players = 0;
int morph_detail = MEDIUM_DETAIL;

struct player_keys
{
    int joy, left, right, up, down, b1, b2, b3, b4;
    // Alternate keys to allow two key bindings for the same action
    int left_2, right_2, up_2, down_2;
};


// Only referenced by the commented-out block below, kept as upstream left it.
[[maybe_unused]] static int binding_for_player( int player )
{
    char tmp[40];
    snprintf( tmp, 40, "player%d", player );
    LSymbol *f = LSymbol::Find(tmp);
    if( !NILP(f) && DEFINEDP(f->GetValue()))
    {
        void *what = f->GetValue();
        if(what == LSymbol::FindOrCreate("keyboard"))
            return 1;
        else if(what == LSymbol::FindOrCreate("joystick"))
            return 2;
    }
    return 0;
}

/*
int get_key_binding(char const *dir, int i)
{
    char tmp[100], kn[50];
    snprintf( tmp, 100, "player%d-%s", i, dir );
    Cell *f = find_symbol( tmp );
    if( NILP(f) || !DEFINEDP( symbol_value( f ) ) )
        return 0;
    void *k = symbol_value( f );

    if( item_type( k ) != L_SYMBOL )
        return 0;

    strcpy( tmp, lstring_value( symbol_name( k ) ) );

    for( char *c = tmp; *c; c++ )
    {
        *c = tolower( *c );
        if( *c == '_' )
            *c = ' ';
    }

    for( int j = 0; j < JK_MAX_KEY; j++ )
    {
        key_name( j, kn );
        for( char *c = kn; *c; c++ )
        {
            *c = tolower(*c);
        }
        if( !strcmp( kn, tmp ) )
            return j;
    }
    return 0;
}
*/

/*
void get_key_bindings()
{
    if( key_map )
    {
        free( key_map );
    }
    key_map = NULL;

    for( key_players = 0; binding_for_player( key_players + 1); key_players++ );
    if( key_players )
    {
        key_map = ( player_keys *)malloc(sizeof(player_keys)*key_players);
        for( int i = 0; i < key_players; i++ )
        {
            key_map[i].joy = ( binding_for_player( i + 1 ) == 2 );
            if( !key_map[i].joy )
            {
                key_map[i].left = get_key_binding( "left", i + 1 );
                key_map[i].right = get_key_binding( "right", i + 1 );
                key_map[i].up = get_key_binding( "up", i + 1 );
                key_map[i].down = get_key_binding( "down", i + 1 );
                key_map[i].b4 = get_key_binding( "b4", i + 1 );
                key_map[i].b3 = get_key_binding( "b3", i + 1 );
                key_map[i].b2 = get_key_binding( "b2", i + 1 );
                key_map[i].b1 = get_key_binding( "b1", i + 1 );
            }
        }
    }
    else
    {
        key_map = NULL;
    }
}*/

namespace {

abuse::input::ActionMap g_actions;

// Set by the first `bind=` line seen: an abuserc that uses the new form
// replaces the defaults entirely rather than adding to them, so a player who
// unbinds something really loses it.
bool g_explicit_binds = false;

// keypreset= is read while parsing abuserc, but the map is only built later,
// by get_key_bindings(). Remembering the choice and applying it at the end of
// the rebuild is what keeps it from being overwritten.
bool g_has_key_preset = false;
abuse::input::KeyPreset g_key_preset = abuse::input::KeyPreset::Classic;

// Asks the engine whether a binding is down. Only keys are wired today; the
// pad and mouse arms land with task 3.3 and need no change here.
bool probe_binding(abuse::input::Binding const &b, void *)
{
    using abuse::input::Source;
    switch (b.source)
    {
    case Source::Key:
        return the_game->key_down(b.code) != 0;
    case Source::PadButton:
        return abuse::input::pad_state().button(b.code);
    case Source::PadAxis:
        // Per axis: the trigger uses a much smaller cutoff than the stick.
        return abuse::input::pad_state().axis_active(
                   b.code, b.sign, abuse::input::deadzone_for_axis(b.code));
    case Source::MouseButton:
    case Source::None:
        break;
    }
    return false;
}

// Mirrors the upstream keys_struct into the action map. A binding of zero
// means "unset" in the old config, and must not become a binding to key 0.
void rebuild_action_map()
{
    using namespace abuse::input;

    // An abuserc with `bind=` lines has already said what it wants; rebuilding
    // from the legacy keys would throw that away.
    if (g_explicit_binds)
        return;

    g_actions = ActionMap();

    auto bind = [](Action a, int code)
    {
        if (code == 0)
            return;
        Binding b;
        b.source = Source::Key;
        b.code = code;
        if (!g_actions.add(a, b))
            printf("Input: no room for key %d on '%s'\n", code, action_name(a));
    };

    bind(Action::Left,  get_key_binding("left", 1));
    bind(Action::Left,  get_key_binding("left2", 1));
    bind(Action::Right, get_key_binding("right", 1));
    bind(Action::Right, get_key_binding("right2", 1));
    bind(Action::Up,    get_key_binding("up", 1));
    bind(Action::Up,    get_key_binding("up2", 1));
    bind(Action::Down,  get_key_binding("down", 1));
    bind(Action::Down,  get_key_binding("down2", 1));

    bind(Action::Special,    get_key_binding("b1", 1));
    bind(Action::Fire,       get_key_binding("b2", 1));
    bind(Action::WeaponPrev, get_key_binding("b3", 1));
    bind(Action::WeaponNext, get_key_binding("b4", 1));

    // Gamepad defaults, added alongside the keys rather than instead of them,
    // which is the point of allowing several bindings: a pad plugged in does
    // not disable the keyboard.
    auto pad_button = [](Action a, int button)
    {
        Binding b;
        b.source = Source::PadButton;
        b.code = button;
        if (!g_actions.add(a, b))
            printf("Input: no room for pad button %d on '%s'\n",
                   button, action_name(a));
    };
    auto pad_axis = [](Action a, int axis, int sign)
    {
        Binding b;
        b.source = Source::PadAxis;
        b.code = axis;
        b.sign = sign;
        if (!g_actions.add(a, b))
            printf("Input: no room for pad axis %d on '%s'\n",
                   axis, action_name(a));
    };

    pad_button(Action::Left,  SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    pad_button(Action::Right, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    pad_button(Action::Up,    SDL_GAMEPAD_BUTTON_DPAD_UP);
    pad_button(Action::Down,  SDL_GAMEPAD_BUTTON_DPAD_DOWN);

    pad_axis(Action::Left,  SDL_GAMEPAD_AXIS_LEFTX, -1);
    pad_axis(Action::Right, SDL_GAMEPAD_AXIS_LEFTX,  1);
    pad_axis(Action::Up,    SDL_GAMEPAD_AXIS_LEFTY, -1);
    pad_axis(Action::Down,  SDL_GAMEPAD_AXIS_LEFTY,  1);

    // Right trigger fires, as the phase specifies; A jumps, which in this game
    // is "up"; B is the special weapon; shoulders change weapon.
    pad_axis(Action::Fire, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1);
    pad_button(Action::Up,         SDL_GAMEPAD_BUTTON_SOUTH);
    pad_button(Action::Special,    SDL_GAMEPAD_BUTTON_EAST);
    pad_button(Action::WeaponPrev, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    pad_button(Action::WeaponNext, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

    // The special is the right mouse button on a mouse, and a trigger is what
    // a hand reaches for to mirror the one that fires. B keeps it too, for a
    // pad whose triggers are buttons.
    pad_axis(Action::Special, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 1);

    // Last, so it replaces the legacy keys while keeping the pad bindings.
    if (g_has_key_preset)
        apply_key_preset(g_key_preset, g_actions);
}

}

abuse::input::ActionMap const &action_map()
{
    return g_actions;
}

abuse::input::ActionMap &mutable_action_map()
{
    return g_actions;
}

void mark_explicit_binds()
{
    g_explicit_binds = true;
}

namespace {

// Pad names as SDL spells them, with a trailing + or - for an axis half.
bool resolve_pad_name(char const *name, abuse::input::Source &source,
                      int &code, int &sign)
{
    using abuse::input::Source;
    size_t len = strlen(name);
    if (len == 0)
        return false;

    char last = name[len - 1];
    if (last == '+' || last == '-')
    {
        char base[64];
        if (len - 1 >= sizeof(base))
            return false;
        memcpy(base, name, len - 1);
        base[len - 1] = 0;

        SDL_GamepadAxis axis = SDL_GetGamepadAxisFromString(base);
        if (axis == SDL_GAMEPAD_AXIS_INVALID)
            return false;
        source = Source::PadAxis;
        code = (int)axis;
        sign = last == '+' ? 1 : -1;
        return true;
    }

    SDL_GamepadButton button = SDL_GetGamepadButtonFromString(name);
    if (button == SDL_GAMEPAD_BUTTON_INVALID)
        return false;
    source = Source::PadButton;
    code = (int)button;
    sign = 0;
    return true;
}

bool write_pad_name(abuse::input::Source source, int code, int sign,
                    char *out, int out_size)
{
    using abuse::input::Source;
    if (source == Source::PadButton)
    {
        char const *n = SDL_GetGamepadStringForButton((SDL_GamepadButton)code);
        if (!n)
            return false;
        snprintf(out, out_size, "%s", n);
        return true;
    }
    if (source == Source::PadAxis)
    {
        char const *n = SDL_GetGamepadStringForAxis((SDL_GamepadAxis)code);
        if (!n)
            return false;
        snprintf(out, out_size, "%s%c", n, sign < 0 ? '-' : '+');
        return true;
    }
    return false;
}

}

void describe_binding(abuse::input::Binding const &b, char *out, int out_size)
{
    using namespace abuse::input;
    out[0] = 0;

    switch (b.source)
    {
    case Source::Key:
    {
        char name[64];
        key_name(b.code, name);
        snprintf(out, out_size, "%s", name);
        break;
    }
    case Source::MouseButton:
        snprintf(out, out_size, "mouse %d", b.code);
        break;
    case Source::PadButton:
    {
        // Under the label this pad prints, because telling a DualSense player
        // to press A is telling them to press the wrong button.
        char const *label = button_label(pad_family(), b.code);
        if (label)
            snprintf(out, out_size, "%s", label);
        else
        {
            char const *n = SDL_GetGamepadStringForButton((SDL_GamepadButton)b.code);
            snprintf(out, out_size, "%s", n ? n : "?");
        }
        break;
    }
    case Source::PadAxis:
    {
        char name[64];
        if (write_pad_name(b.source, b.code, b.sign, name, sizeof(name)))
            snprintf(out, out_size, "%s", name);
        else
            snprintf(out, out_size, "?");
        break;
    }
    case Source::None:
        break;
    }
}

std::vector<std::string> format_all_bindings()
{
    using namespace abuse::input;
    std::vector<std::string> lines;

    for (int i = 0; i < (int)Action::Count; i++)
    {
        Action a = (Action)i;
        for (int j = 0; j < kMaxBindings; j++)
        {
            Binding const &b = g_actions.binding(a, j);
            if (b.empty())
                continue;
            char line[128];
            if (format_bind_line(a, b, write_pad_name, line, sizeof(line)))
            {
                // format_bind_line writes the whole "bind=..." form; the file
                // writer adds the key, so hand it only the value.
                char const *eq = strchr(line, '=');
                lines.push_back(eq ? eq + 1 : line);
            }
        }
    }

    return lines;
}

// Applies one `bind=` line from abuserc. Returns false when the line is
// malformed, so the caller can say which line was dropped.
bool add_config_binding(char const *spec)
{
    using namespace abuse::input;

    Action a;
    Binding b;
    if (!parse_bind_line(spec, resolve_pad_name, a, b))
        return false;

    if (!g_explicit_binds)
    {
        // First explicit bind wins over the whole default set.
        for (int i = 0; i < (int)Action::Count; i++)
            g_actions.clear((Action)i);
        g_explicit_binds = true;
    }

    return g_actions.add(a, b);
}

bool config_has_explicit_binds()
{
    return g_explicit_binds;
}

// keypreset= in abuserc. Applied after the legacy keys are read, so it wins
// over them, and before the pad defaults, which it keeps.
bool apply_config_key_preset(char const *name)
{
    using namespace abuse::input;
    KeyPreset p;
    if (!parse_key_preset(name, p))
        return false;
    g_key_preset = p;
    g_has_key_preset = true;
    return true;
}

void get_key_bindings()
{
    key_players = 1;
    rebuild_action_map();
}

// The replay suite does not exercise input resolution: playback overwrites the
// packet that get_movement fills. This is how a config can be checked without
// a keyboard.
void print_action_map()
{
    using namespace abuse::input;
    printf("action map:\n");
    for (int i = 0; i < (int)Action::Count; i++)
    {
        Action a = (Action)i;
        printf("  %-10s", action_name(a));
        if (g_actions.binding_count(a) == 0)
            printf(" (unbound)");
        for (int j = 0; j < kMaxBindings; j++)
        {
            Binding const &b = g_actions.binding(a, j);
            if (b.empty())
                continue;
            char name[64];
            switch (b.source)
            {
            case Source::Key:
                key_name(b.code, name);
                printf("  tecla:%s", name);
                break;
            case Source::PadButton:
                printf("  botao:%s",
                       SDL_GetGamepadStringForButton((SDL_GamepadButton)b.code));
                break;
            case Source::PadAxis:
                printf("  eixo:%s%s", b.sign < 0 ? "-" : "+",
                       SDL_GetGamepadStringForAxis((SDL_GamepadAxis)b.code));
                break;
            case Source::MouseButton:
                printf("  mouse:%d", b.code);
                break;
            case Source::None:
                break;
            }
        }
        printf("\n");
    }
    fflush(stdout);
}


#define is_pressed(x) the_game->key_down(x)

void get_movement(int player, int &x, int &y, int &b1, int &b2, int &b3, int &b4)
{
    // A scripted player, when the harness was given one. Before the check
    // below and not after: --headless skips readRCFile, so key_players is
    // still zero there and the real path would answer "no input" before the
    // script was ever asked. It produces the same byte the action map would,
    // which is why nothing downstream, --record included, can tell the
    // difference. See abuse::harness::scripted_input.
    {
        uint8_t scripted = 0;
        if( abuse::harness::scripted_input( scripted ) )
        {
            x  = (scripted & 1)  ? 1 : ((scripted & 2) ? -1 : 0);
            y  = (scripted & 4)  ? 1 : ((scripted & 8) ? -1 : 0);
            b1 = (scripted & 16) ? 1 : 0;
            b2 = (scripted & 32) ? 1 : 0;
            b3 = (scripted & 64) ? 1 : 0;
            b4 = (scripted & 128) ? 1 : 0;
            return;
        }
    }

    if( player >= key_players )
    {
        // FIXME: inherited oddity, b4 keeps its previous value.
        x = y = b1 = b2 = b3 = 0;
        return;
    }

    // Phase 3, task 3.2: resolved through the action map, which allows more
    // than one binding per action and takes pad and mouse in task 3.3. The
    // packet layout lives in abuse::input::to_flags; the engine still wants
    // the values unpacked, so they are unpacked here and nowhere else.
    using namespace abuse::input;

    bool pressed[(int)Action::Count];
    resolve( g_actions, probe_binding, NULL, pressed );

    uint8_t flags = to_flags( pressed );
    x  = (flags & 1)  ? 1 : ((flags & 2) ? -1 : 0);
    y  = (flags & 4)  ? 1 : ((flags & 8) ? -1 : 0);
    b1 = (flags & 16) ? 1 : 0;
    b2 = (flags & 32) ? 1 : 0;
    b3 = (flags & 64) ? 1 : 0;
    b4 = (flags & 128) ? 1 : 0;
}

int consume_weapon_change()
{
    using namespace abuse::input;

    static bool was_prev = false;
    static bool was_next = false;

    bool pressed[(int)Action::Count];
    resolve( g_actions, probe_binding, NULL, pressed );

    bool prev = pressed[(int)Action::WeaponPrev];
    bool next = pressed[(int)Action::WeaponNext];

    int dir = 0;
    if( prev && !was_prev )
        dir = -1;
    else if( next && !was_next )
        dir = 1;

    was_prev = prev;
    was_next = next;
    return dir;
}

/*
This doesn't appear to be used
void key_bindings(int player, int &left, int &right, int &up, int &down, int &b1, int &b2, int &b3, int &b4)
{
    left = key_map[player].left;
    right = key_map[player].right;
    up = key_map[player].up;
    down = key_map[player].down;
    b1 = key_map[player].b1;
    b2 = key_map[player].b2;
    b3 = key_map[player].b3;
    b3 = key_map[player].b4;
}
*/

void config_cleanup()
{
    // Nothing to release since the key map became an ActionMap with static
    // storage. Kept because game.cpp calls it.
}

//
// Get the keycode for the string 'str'
// Returns -1 for an invalid key code
//
int get_keycode(char const *str)
{
    if( !str[0] )
    {
        return -1;
    }
    else if( !str[1] )
    {
        return str[0];
    }
    else
    {
        int j;
        char buf[20];
        for( j = 256; j < JK_MAX_KEY; j++ )
        {
            key_name( j, buf );
            char *c = buf;
            for( ; *c; c++ )
            {
                if( *c == ' ' )
                {
                    *c = '_';
                }
                else
                {
                    *c = tolower( *c );
                }
            }
            if( strcmp( str, buf ) == 0 )
            {
                return j;
            }
        }
    }
    return -1;
}
