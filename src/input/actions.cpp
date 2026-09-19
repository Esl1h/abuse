/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See actions.h.
 *
 *  This software was released into the Public Domain.
 */

#include "actions.h"

#include <stdio.h>
#include <string.h>
#include "compat.h"

#include "keys.h"

namespace abuse::input {

namespace {

// Names as written in abuserc. "fire" and "special" are the historical names
// for what the packet calls b2 and b1; keeping them means an existing config
// keeps working.
struct NameEntry { Action action; char const *name; };

NameEntry const kNames[] = {
    { Action::Left,       "left"      },
    { Action::Right,      "right"     },
    { Action::Up,         "up"        },
    { Action::Down,       "down"      },
    { Action::Fire,       "fire"      },
    { Action::Special,    "special"   },
    { Action::WeaponPrev, "weapprev"  },
    { Action::WeaponNext, "weapnext"  },
};

static_assert(sizeof(kNames) / sizeof(kNames[0]) == (size_t)Action::Count,
              "every action needs a name");

}

char const *action_name(Action a)
{
    for (NameEntry const &e : kNames)
        if (e.action == a)
            return e.name;
    return "?";
}

bool parse_action(char const *name, Action &out)
{
    if (!name)
        return false;
    for (NameEntry const &e : kNames)
        if (strcasecmp(e.name, name) == 0)
        {
            out = e.action;
            return true;
        }
    return false;
}

ActionMap::ActionMap() = default;

void ActionMap::clear(Action a)
{
    for (int i = 0; i < kMaxBindings; i++)
        m_bindings[(int)a][i] = Binding();
}

bool ActionMap::add(Action a, Binding b)
{
    if (b.empty())
        return false;
    for (int i = 0; i < kMaxBindings; i++)
        if (m_bindings[(int)a][i].empty())
        {
            m_bindings[(int)a][i] = b;
            return true;
        }
    return false;
}

int ActionMap::binding_count(Action a) const
{
    int n = 0;
    for (int i = 0; i < kMaxBindings; i++)
        if (!m_bindings[(int)a][i].empty())
            n++;
    return n;
}

Binding const &ActionMap::binding(Action a, int index) const
{
    static Binding const none;
    if (index < 0 || index >= kMaxBindings)
        return none;
    return m_bindings[(int)a][index];
}

bool ActionMap::has(Action a, Source source, int code) const
{
    for (int i = 0; i < kMaxBindings; i++)
    {
        Binding const &b = m_bindings[(int)a][i];
        if (b.source == source && b.code == code)
            return true;
    }
    return false;
}

void ActionMap::set_legacy_defaults()
{
    for (int i = 0; i < (int)Action::Count; i++)
        clear((Action)i);

    auto key = [](int code) { Binding b; b.source = Source::Key; b.code = code; return b; };

    // The same defaults setup() has always installed.
    add(Action::Left,  key(JK_LEFT));
    add(Action::Left,  key('a'));
    add(Action::Right, key(JK_RIGHT));
    add(Action::Right, key('d'));
    add(Action::Up,    key(JK_UP));
    add(Action::Up,    key('w'));
    add(Action::Down,  key(JK_DOWN));
    add(Action::Down,  key('s'));

    add(Action::Fire,       key(JK_SPACE));
    add(Action::WeaponPrev, key(JK_CTRL_R));
    add(Action::WeaponNext, key(JK_INSERT));
    // Special has no default key upstream: the config leaves b1 at zero
    // unless abuserc sets it. Left empty on purpose rather than invented.
}

namespace {

// Copies one comma-separated field, trimming spaces. Returns the position
// after the comma, or NULL when there is no field left.
char const *take_field(char const *p, char *out, int out_size)
{
    if (!p)
        return NULL;
    while (*p == ' ' || *p == '\t')
        p++;

    int n = 0;
    while (*p && *p != ',' && *p != '\n' && *p != '\r')
    {
        if (n < out_size - 1)
            out[n++] = *p;
        p++;
    }
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t'))
        n--;
    out[n] = 0;

    if (n == 0)
        return NULL;
    return *p == ',' ? p + 1 : p;
}

}

bool parse_bind_line(char const *text, PadNameResolver resolver,
                     Action &action, Binding &binding)
{
    if (!text)
        return false;

    char field[64];
    char const *p = take_field(text, field, sizeof(field));

    Action a;
    if (!p || !parse_action(field, a))
        return false;

    p = take_field(p, field, sizeof(field));
    if (!p)
        return false;

    Binding b;
    if (strcasecmp(field, "key") == 0)
    {
        if (!take_field(p, field, sizeof(field)))
            return false;
        int code = key_value(field);
        // key_value falls through to the first character for anything it does
        // not recognise, so an empty name would silently bind to nothing.
        if (code == 0)
            return false;
        b.source = Source::Key;
        b.code = code;
    }
    else if (strcasecmp(field, "mouse") == 0)
    {
        if (!take_field(p, field, sizeof(field)))
            return false;
        int code = 0;
        for (char const *c = field; *c; c++)
        {
            if (*c < '0' || *c > '9')
                return false;
            code = code * 10 + (*c - '0');
        }
        if (code < 1 || code > 8)
            return false;
        b.source = Source::MouseButton;
        b.code = code;
    }
    else if (strcasecmp(field, "pad") == 0)
    {
        if (!take_field(p, field, sizeof(field)) || !resolver)
            return false;
        Source src = Source::None;
        int code = 0, sign = 0;
        if (!resolver(field, src, code, sign))
            return false;
        b.source = src;
        b.code = code;
        b.sign = sign;
    }
    else
        return false;

    action = a;
    binding = b;
    return true;
}

bool format_bind_line(Action action, Binding const &b, PadNameWriter writer,
                      char *out, int out_size)
{
    if (!out || out_size <= 0 || b.empty())
        return false;

    char name[64];
    switch (b.source)
    {
    case Source::Key:
        key_name(b.code, name);
        if (!name[0])
            return false;
        snprintf(out, out_size, "bind=%s,key,%s", action_name(action), name);
        return true;
    case Source::MouseButton:
        snprintf(out, out_size, "bind=%s,mouse,%d", action_name(action), b.code);
        return true;
    case Source::PadButton:
    case Source::PadAxis:
        if (!writer || !writer(b.source, b.code, b.sign, name, sizeof(name)))
            return false;
        snprintf(out, out_size, "bind=%s,pad,%s", action_name(action), name);
        return true;
    case Source::None:
        break;
    }
    return false;
}

bool parse_key_preset(char const *name, KeyPreset &out)
{
    if (!name)
        return false;
    if (strcasecmp(name, "classic") == 0)
        out = KeyPreset::Classic;
    else if (strcasecmp(name, "modern") == 0)
        out = KeyPreset::Modern;
    else
        return false;
    return true;
}

char const *key_preset_name(KeyPreset p)
{
    return p == KeyPreset::Modern ? "modern" : "classic";
}

void apply_key_preset(KeyPreset p, ActionMap &map)
{
    // Only the keys: whatever the pad had stays.
    Binding kept[(int)Action::Count][kMaxBindings];
    for (int i = 0; i < (int)Action::Count; i++)
        for (int j = 0; j < kMaxBindings; j++)
            kept[i][j] = map.binding((Action)i, j);

    for (int i = 0; i < (int)Action::Count; i++)
        map.clear((Action)i);

    auto key = [&](Action a, int code)
    {
        if (code == 0)
            return;
        Binding b;
        b.source = Source::Key;
        b.code = code;
        map.add(a, b);
    };

    key(Action::Left,  JK_LEFT);
    key(Action::Left,  'a');
    key(Action::Right, JK_RIGHT);
    key(Action::Right, 'd');
    key(Action::Up,    JK_UP);
    key(Action::Up,    'w');
    key(Action::Down,  JK_DOWN);
    key(Action::Down,  's');
    key(Action::Fire,  JK_SPACE);

    if (p == KeyPreset::Modern)
    {
        // Weapon switching next to the movement hand, and a real key for the
        // special weapon, which classic leaves unbound.
        key(Action::WeaponPrev, 'q');
        key(Action::WeaponNext, 'e');
        key(Action::Special,    'f');
    }
    else
    {
        key(Action::WeaponPrev, JK_CTRL_R);
        key(Action::WeaponNext, JK_INSERT);
        // Special stays unbound, as upstream ships it.
    }

    // Put the pad bindings back.
    for (int i = 0; i < (int)Action::Count; i++)
        for (int j = 0; j < kMaxBindings; j++)
            if (kept[i][j].source == Source::PadButton
                || kept[i][j].source == Source::PadAxis
                || kept[i][j].source == Source::MouseButton)
                map.add((Action)i, kept[i][j]);
}

void resolve(ActionMap const &map, BindingProbe probe, void *user,
             bool pressed[(int)Action::Count])
{
    for (int i = 0; i < (int)Action::Count; i++)
    {
        Action a = (Action)i;
        pressed[i] = false;
        if (!probe)
            continue;
        for (int j = 0; j < kMaxBindings; j++)
        {
            Binding const &b = map.binding(a, j);
            // Any binding is enough: a player with a key and a pad button on
            // the same action expects either to work.
            if (!b.empty() && probe(b, user))
            {
                pressed[i] = true;
                break;
            }
        }
    }
}

uint8_t to_flags(bool const pressed[(int)Action::Count])
{
    // The layout of SCMD_SET_INPUT, spelled out. view.cpp writes these bits and
    // view::process_input reads them back; both sides have to agree with this.
    // Precedence matters and is not symmetric: get_movement tests left before
    // right and up before down, so pressing both gives left and up. Verified
    // against the engine formula over all 256 combinations.
    uint8_t flags = 0;
    if (pressed[(int)Action::Left])        flags |= 2;
    else if (pressed[(int)Action::Right])  flags |= 1;

    if (pressed[(int)Action::Up])          flags |= 8;
    else if (pressed[(int)Action::Down])   flags |= 4;

    // b1 is "special" and b2 is "fire", not the other way round: setup.cpp
    // maps the abuserc name "special" to keys.b1 and "fire" to keys.b2.
    if (pressed[(int)Action::Special])    flags |= 16;
    if (pressed[(int)Action::Fire])       flags |= 32;
    if (pressed[(int)Action::WeaponPrev]) flags |= 64;
    if (pressed[(int)Action::WeaponNext]) flags |= 128;

    return flags;
}

}
