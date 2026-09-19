#include <doctest/doctest.h>

#include <stdio.h>
#include <string.h>

#include "input/actions.h"
#include "keys.h"

using namespace abuse::input;

namespace {

struct Pressed
{
    bool v[(int)Action::Count] = {};
    bool const *operator&() const { return v; }
    void set(Action a) { v[(int)a] = true; }
};

}

TEST_CASE("every action has a name and round trips") {
    for (int i = 0; i < (int)Action::Count; i++)
    {
        Action a = (Action)i;
        char const *name = action_name(a);
        REQUIRE(strcmp(name, "?") != 0);

        Action back;
        REQUIRE(parse_action(name, back));
        CHECK(back == a);
    }
}

TEST_CASE("action names are the ones abuserc already uses") {
    // Renaming these breaks every existing config file.
    CHECK(strcmp(action_name(Action::Fire), "fire") == 0);
    CHECK(strcmp(action_name(Action::Special), "special") == 0);
    CHECK(strcmp(action_name(Action::WeaponPrev), "weapprev") == 0);
    CHECK(strcmp(action_name(Action::WeaponNext), "weapnext") == 0);
}

TEST_CASE("unknown action names are rejected") {
    Action a = Action::Fire;
    CHECK_FALSE(parse_action("jump", a));
    CHECK_FALSE(parse_action("", a));
    CHECK_FALSE(parse_action(nullptr, a));
    CHECK(a == Action::Fire);
}

// The wire layout of SCMD_SET_INPUT. These numbers are not a choice: they are
// what src/view.cpp writes and view::process_input reads, and every recorded
// replay is a stream of them. A change here silently invalidates the replays.
TEST_CASE("flags match the packet layout of SCMD_SET_INPUT") {
    Pressed p;

    CHECK(to_flags(&p) == 0);

    p = Pressed(); p.set(Action::Right);      CHECK(to_flags(&p) == 1);
    p = Pressed(); p.set(Action::Left);       CHECK(to_flags(&p) == 2);
    p = Pressed(); p.set(Action::Down);       CHECK(to_flags(&p) == 4);
    p = Pressed(); p.set(Action::Up);         CHECK(to_flags(&p) == 8);
    p = Pressed(); p.set(Action::Special);    CHECK(to_flags(&p) == 16);
    p = Pressed(); p.set(Action::Fire);       CHECK(to_flags(&p) == 32);
    p = Pressed(); p.set(Action::WeaponPrev); CHECK(to_flags(&p) == 64);
    p = Pressed(); p.set(Action::WeaponNext); CHECK(to_flags(&p) == 128);
}

// get_movement uses an if/else chain that tests left before right and up
// before down, so pressing both opposites gives left and up. The first version
// of this test asserted the opposite and passed, because the code had the same
// mistake; only a cross-check against the engine formula caught it.
TEST_CASE("opposite directions resolve the way the engine does") {
    Pressed p;
    p.set(Action::Left);
    p.set(Action::Right);
    CHECK(to_flags(&p) == 2);   // left wins

    Pressed q;
    q.set(Action::Up);
    q.set(Action::Down);
    CHECK(to_flags(&q) == 8);   // cima vence
}

TEST_CASE("buttons combine, directions do not") {
    Pressed p;
    p.set(Action::Fire);
    p.set(Action::Special);
    p.set(Action::Right);
    CHECK(to_flags(&p) == (1 | 16 | 32));
}

TEST_CASE("a fresh map has no bindings") {
    ActionMap m;
    for (int i = 0; i < (int)Action::Count; i++)
        CHECK(m.binding_count((Action)i) == 0);
}

TEST_CASE("an action takes several bindings from different devices") {
    ActionMap m;
    Binding k; k.source = Source::Key; k.code = JK_SPACE;
    Binding pad; pad.source = Source::PadButton; pad.code = 7;
    Binding mouse; mouse.source = Source::MouseButton; mouse.code = 1;

    REQUIRE(m.add(Action::Fire, k));
    REQUIRE(m.add(Action::Fire, pad));
    REQUIRE(m.add(Action::Fire, mouse));

    CHECK(m.binding_count(Action::Fire) == 3);
    CHECK(m.has(Action::Fire, Source::Key, JK_SPACE));
    CHECK(m.has(Action::Fire, Source::PadButton, 7));
    CHECK(m.has(Action::Fire, Source::MouseButton, 1));
    CHECK_FALSE(m.has(Action::Fire, Source::Key, JK_ENTER));
    // Same code, different device, is a different binding.
    CHECK_FALSE(m.has(Action::Fire, Source::PadButton, 1));
}

// Silently dropping a binding would leave the player with a rebind screen that
// appears to work and does nothing.
TEST_CASE("adding past the limit fails instead of dropping quietly") {
    ActionMap m;
    for (int i = 0; i < kMaxBindings; i++)
    {
        Binding b; b.source = Source::Key; b.code = 'a' + i;
        REQUIRE(m.add(Action::Fire, b));
    }
    CHECK(m.binding_count(Action::Fire) == kMaxBindings);

    Binding extra; extra.source = Source::Key; extra.code = 'z';
    CHECK_FALSE(m.add(Action::Fire, extra));
    CHECK(m.binding_count(Action::Fire) == kMaxBindings);
    CHECK_FALSE(m.has(Action::Fire, Source::Key, 'z'));
}

TEST_CASE("empty bindings are refused") {
    ActionMap m;
    CHECK_FALSE(m.add(Action::Fire, Binding()));
    CHECK(m.binding_count(Action::Fire) == 0);
}

TEST_CASE("clear removes every binding of one action only") {
    ActionMap m;
    Binding b; b.source = Source::Key; b.code = JK_SPACE;
    m.add(Action::Fire, b);
    m.add(Action::Left, b);

    m.clear(Action::Fire);
    CHECK(m.binding_count(Action::Fire) == 0);
    CHECK(m.binding_count(Action::Left) == 1);
}

// The upstream defaults, which an existing abuserc expects to find.
TEST_CASE("legacy defaults reproduce the upstream bindings") {
    ActionMap m;
    m.set_legacy_defaults();

    CHECK(m.has(Action::Left,  Source::Key, JK_LEFT));
    CHECK(m.has(Action::Left,  Source::Key, 'a'));
    CHECK(m.has(Action::Right, Source::Key, JK_RIGHT));
    CHECK(m.has(Action::Right, Source::Key, 'd'));
    CHECK(m.has(Action::Up,    Source::Key, JK_UP));
    CHECK(m.has(Action::Up,    Source::Key, 'w'));
    CHECK(m.has(Action::Down,  Source::Key, JK_DOWN));
    CHECK(m.has(Action::Down,  Source::Key, 's'));

    CHECK(m.has(Action::Fire,       Source::Key, JK_SPACE));
    CHECK(m.has(Action::WeaponPrev, Source::Key, JK_CTRL_R));
    CHECK(m.has(Action::WeaponNext, Source::Key, JK_INSERT));

    // Upstream leaves the special weapon unbound; inventing a key here would
    // change the controls of an existing install.
    CHECK(m.binding_count(Action::Special) == 0);
}

// Exhaustive cross-check against the flag assembly as src/view.cpp writes it.
// Kept as a copy on purpose: if someone edits either side, the two stop
// agreeing and this fails. Asserting to_flags against itself would prove
// nothing, which is how the direction precedence was wrong and green at once.
namespace {

uint8_t engine_flags(int x, int y, int b1, int b2, int b3, int b4)
{
    uint8_t m = 0;
    if (x > 0) m |= 1; else if (x < 0) m |= 2;
    if (y > 0) m |= 4; else if (y < 0) m |= 8;
    if (b1) m |= 16;
    if (b2) m |= 32;
    if (b3) m |= 64;
    if (b4) m |= 128;
    return m;
}

}

TEST_CASE("to_flags agrees with the engine over every combination") {
    int checked = 0;
    for (int L = 0; L < 2; L++)
    for (int R = 0; R < 2; R++)
    for (int U = 0; U < 2; U++)
    for (int D = 0; D < 2; D++)
    for (int sp = 0; sp < 2; sp++)
    for (int fi = 0; fi < 2; fi++)
    for (int wp = 0; wp < 2; wp++)
    for (int wn = 0; wn < 2; wn++)
    {
        // get_movement: left is tested first and wins; right only in the else.
        int x = L ? -1 : (R ? 1 : 0);
        int y = U ? -1 : (D ? 1 : 0);

        Pressed p;
        if (L)  p.set(Action::Left);
        if (R)  p.set(Action::Right);
        if (U)  p.set(Action::Up);
        if (D)  p.set(Action::Down);
        if (sp) p.set(Action::Special);
        if (fi) p.set(Action::Fire);
        if (wp) p.set(Action::WeaponPrev);
        if (wn) p.set(Action::WeaponNext);

        REQUIRE(to_flags(&p) == engine_flags(x, y, sp, fi, wp, wn));
        checked++;
    }
    CHECK(checked == 256);
}

namespace {

// Probe over a fixed list of "active" bindings, so resolution can be tested
// without a keyboard, a mouse or a pad.
struct FakeDevice
{
    Binding active[8];
    int count = 0;

    void press(Source s, int code) { active[count].source = s; active[count].code = code; count++; }

    static bool probe(Binding const &b, void *user)
    {
        FakeDevice *d = (FakeDevice *)user;
        for (int i = 0; i < d->count; i++)
            if (d->active[i].source == b.source && d->active[i].code == b.code)
                return true;
        return false;
    }
};

}

TEST_CASE("resolve reports an action when any of its bindings is active") {
    ActionMap m;
    m.set_legacy_defaults();

    FakeDevice dev;
    dev.press(Source::Key, 'a');          // segundo binding de Left

    bool pressed[(int)Action::Count];
    resolve(m, FakeDevice::probe, &dev, pressed);

    CHECK(pressed[(int)Action::Left]);
    CHECK_FALSE(pressed[(int)Action::Right]);
    CHECK_FALSE(pressed[(int)Action::Fire]);
    CHECK(to_flags(pressed) == 2);
}

TEST_CASE("a pad button drives the same action as the key") {
    ActionMap m;
    Binding k; k.source = Source::Key; k.code = JK_SPACE;
    Binding pad; pad.source = Source::PadButton; pad.code = 0;
    m.add(Action::Fire, k);
    m.add(Action::Fire, pad);

    bool pressed[(int)Action::Count];

    FakeDevice keyboard;
    keyboard.press(Source::Key, JK_SPACE);
    resolve(m, FakeDevice::probe, &keyboard, pressed);
    CHECK(pressed[(int)Action::Fire]);

    FakeDevice gamepad;
    gamepad.press(Source::PadButton, 0);
    resolve(m, FakeDevice::probe, &gamepad, pressed);
    CHECK(pressed[(int)Action::Fire]);

    FakeDevice idle;
    resolve(m, FakeDevice::probe, &idle, pressed);
    CHECK_FALSE(pressed[(int)Action::Fire]);
}

TEST_CASE("resolve clears actions with no binding") {
    ActionMap m;
    m.set_legacy_defaults();

    FakeDevice dev;
    dev.press(Source::Key, JK_SPACE);

    bool pressed[(int)Action::Count];
    resolve(m, FakeDevice::probe, &dev, pressed);

    CHECK(pressed[(int)Action::Fire]);
    // Special has no default binding; it must read as released, not as noise.
    CHECK_FALSE(pressed[(int)Action::Special]);
}

TEST_CASE("a null probe reports nothing pressed") {
    ActionMap m;
    m.set_legacy_defaults();

    bool pressed[(int)Action::Count];
    resolve(m, nullptr, nullptr, pressed);

    for (int i = 0; i < (int)Action::Count; i++)
        CHECK_FALSE(pressed[i]);
    CHECK(to_flags(pressed) == 0);
}

namespace {

// Stand-in for the SDL name lookup, so the parser can be tested without SDL.
bool fake_pad_resolver(char const *name, Source &source, int &code, int &sign)
{
    if (strcmp(name, "dpleft") == 0)        { source = Source::PadButton; code = 13; sign = 0; return true; }
    if (strcmp(name, "a") == 0)             { source = Source::PadButton; code = 0;  sign = 0; return true; }
    if (strcmp(name, "righttrigger+") == 0) { source = Source::PadAxis;   code = 5;  sign = 1; return true; }
    if (strcmp(name, "leftx-") == 0)        { source = Source::PadAxis;   code = 0;  sign = -1; return true; }
    return false;
}

bool fake_pad_writer(Source source, int code, int sign, char *out, int size)
{
    if (source == Source::PadButton && code == 13) { snprintf(out, size, "dpleft"); return true; }
    if (source == Source::PadAxis && code == 5 && sign > 0) { snprintf(out, size, "righttrigger+"); return true; }
    return false;
}

}

TEST_CASE("bind lines parse for each device") {
    Action a;
    Binding b;

    REQUIRE(parse_bind_line("fire,key,Space", fake_pad_resolver, a, b));
    CHECK(a == Action::Fire);
    CHECK(b.source == Source::Key);
    CHECK(b.code == JK_SPACE);

    REQUIRE(parse_bind_line("left,pad,dpleft", fake_pad_resolver, a, b));
    CHECK(a == Action::Left);
    CHECK(b.source == Source::PadButton);
    CHECK(b.code == 13);

    REQUIRE(parse_bind_line("fire,pad,righttrigger+", fake_pad_resolver, a, b));
    CHECK(b.source == Source::PadAxis);
    CHECK(b.code == 5);
    CHECK(b.sign == 1);

    REQUIRE(parse_bind_line("special,mouse,3", fake_pad_resolver, a, b));
    CHECK(a == Action::Special);
    CHECK(b.source == Source::MouseButton);
    CHECK(b.code == 3);
}

TEST_CASE("spaces around fields are tolerated") {
    Action a;
    Binding b;
    REQUIRE(parse_bind_line("  fire , key , Space ", fake_pad_resolver, a, b));
    CHECK(a == Action::Fire);
    CHECK(b.code == JK_SPACE);
}

// One bad line drops one binding, not the whole config, so both outputs have
// to be left untouched on failure.
TEST_CASE("malformed bind lines are rejected without side effects") {
    Action a = Action::WeaponNext;
    Binding b;
    b.source = Source::Key;
    b.code = 1234;

    CHECK_FALSE(parse_bind_line("", fake_pad_resolver, a, b));
    CHECK_FALSE(parse_bind_line(nullptr, fake_pad_resolver, a, b));
    CHECK_FALSE(parse_bind_line("jump,key,Space", fake_pad_resolver, a, b));   // acao inexistente
    CHECK_FALSE(parse_bind_line("fire", fake_pad_resolver, a, b));             // no device
    CHECK_FALSE(parse_bind_line("fire,key", fake_pad_resolver, a, b));         // no code
    CHECK_FALSE(parse_bind_line("fire,laser,Space", fake_pad_resolver, a, b)); // device that does not exist
    CHECK_FALSE(parse_bind_line("fire,pad,nosuchbutton", fake_pad_resolver, a, b));
    CHECK_FALSE(parse_bind_line("fire,mouse,0", fake_pad_resolver, a, b));     // fora de faixa
    CHECK_FALSE(parse_bind_line("fire,mouse,99", fake_pad_resolver, a, b));
    CHECK_FALSE(parse_bind_line("fire,mouse,x", fake_pad_resolver, a, b));

    CHECK(a == Action::WeaponNext);
    CHECK(b.code == 1234);
}

TEST_CASE("a pad line without a resolver is rejected, not guessed") {
    Action a;
    Binding b;
    CHECK_FALSE(parse_bind_line("left,pad,dpleft", nullptr, a, b));
}

TEST_CASE("bindings format back into the line that produced them") {
    char out[128];

    Binding k; k.source = Source::Key; k.code = JK_SPACE;
    REQUIRE(format_bind_line(Action::Fire, k, fake_pad_writer, out, sizeof(out)));
    CHECK(strcmp(out, "bind=fire,key,Space") == 0);

    Binding pad; pad.source = Source::PadButton; pad.code = 13;
    REQUIRE(format_bind_line(Action::Left, pad, fake_pad_writer, out, sizeof(out)));
    CHECK(strcmp(out, "bind=left,pad,dpleft") == 0);

    Binding axis; axis.source = Source::PadAxis; axis.code = 5; axis.sign = 1;
    REQUIRE(format_bind_line(Action::Fire, axis, fake_pad_writer, out, sizeof(out)));
    CHECK(strcmp(out, "bind=fire,pad,righttrigger+") == 0);

    Binding mouse; mouse.source = Source::MouseButton; mouse.code = 2;
    REQUIRE(format_bind_line(Action::Special, mouse, fake_pad_writer, out, sizeof(out)));
    CHECK(strcmp(out, "bind=special,mouse,2") == 0);
}

TEST_CASE("formatting refuses what it cannot write back") {
    char out[128];
    CHECK_FALSE(format_bind_line(Action::Fire, Binding(), fake_pad_writer, out, sizeof(out)));

    Binding pad; pad.source = Source::PadButton; pad.code = 99;   // the writer does not know it
    CHECK_FALSE(format_bind_line(Action::Fire, pad, fake_pad_writer, out, sizeof(out)));
    CHECK_FALSE(format_bind_line(Action::Fire, pad, nullptr, out, sizeof(out)));
}

// A line written out has to parse back into the same binding, or a config
// saved by the game stops matching the config it loaded.
TEST_CASE("format and parse round trip") {
    struct { Action a; Source s; int code; int sign; } cases[] = {
        { Action::Fire, Source::Key, JK_SPACE, 0 },
        { Action::Left, Source::PadButton, 13, 0 },
        { Action::Fire, Source::PadAxis, 5, 1 },
        { Action::Special, Source::MouseButton, 2, 0 },
    };

    for (auto const &c : cases)
    {
        Binding b; b.source = c.s; b.code = c.code; b.sign = c.sign;
        char line[128];
        REQUIRE(format_bind_line(c.a, b, fake_pad_writer, line, sizeof(line)));

        // A linha gravada tem o prefixo "bind="; o parser recebe o resto.
        char const *payload = strchr(line, '=');
        REQUIRE(payload != nullptr);

        Action back_a;
        Binding back_b;
        REQUIRE(parse_bind_line(payload + 1, fake_pad_resolver, back_a, back_b));
        CHECK(back_a == c.a);
        CHECK(back_b.source == c.s);
        CHECK(back_b.code == c.code);
        CHECK(back_b.sign == c.sign);
    }
}

TEST_CASE("key preset names parse and round trip") {
    KeyPreset p = KeyPreset::Classic;
    REQUIRE(parse_key_preset("modern", p));
    CHECK(p == KeyPreset::Modern);
    CHECK(strcmp(key_preset_name(p), "modern") == 0);

    REQUIRE(parse_key_preset("CLASSIC", p));
    CHECK(p == KeyPreset::Classic);
    CHECK(strcmp(key_preset_name(p), "classic") == 0);

    CHECK_FALSE(parse_key_preset("vim", p));
    CHECK_FALSE(parse_key_preset(nullptr, p));
    CHECK(p == KeyPreset::Classic);
}

// classic must be byte for byte what the game always shipped, including
// leaving the special weapon unbound, or an existing install changes controls
// on upgrade.
TEST_CASE("classic preset equals the legacy defaults") {
    ActionMap legacy;
    legacy.set_legacy_defaults();

    ActionMap preset;
    apply_key_preset(KeyPreset::Classic, preset);

    for (int i = 0; i < (int)Action::Count; i++)
    {
        Action a = (Action)i;
        CHECK(preset.binding_count(a) == legacy.binding_count(a));
        for (int j = 0; j < legacy.binding_count(a); j++)
            CHECK(preset.has(a, legacy.binding(a, j).source,
                             legacy.binding(a, j).code));
    }
    CHECK(preset.binding_count(Action::Special) == 0);
}

TEST_CASE("modern preset moves weapons to Q and E and binds the special") {
    ActionMap m;
    apply_key_preset(KeyPreset::Modern, m);

    CHECK(m.has(Action::WeaponPrev, Source::Key, 'q'));
    CHECK(m.has(Action::WeaponNext, Source::Key, 'e'));
    CHECK(m.has(Action::Special,    Source::Key, 'f'));

    // Movement and fire do not move between presets.
    CHECK(m.has(Action::Left,  Source::Key, 'a'));
    CHECK(m.has(Action::Right, Source::Key, JK_RIGHT));
    CHECK(m.has(Action::Fire,  Source::Key, JK_SPACE));

    // And the classic keys are gone, not merely shadowed.
    CHECK_FALSE(m.has(Action::WeaponPrev, Source::Key, JK_CTRL_R));
    CHECK_FALSE(m.has(Action::WeaponNext, Source::Key, JK_INSERT));
}

// The preset is about the keyboard: a pad binding set before it must survive.
TEST_CASE("applying a preset keeps pad and mouse bindings") {
    ActionMap m;
    Binding pad; pad.source = Source::PadButton; pad.code = 7;
    Binding mouse; mouse.source = Source::MouseButton; mouse.code = 1;
    m.add(Action::Fire, pad);
    m.add(Action::Special, mouse);

    apply_key_preset(KeyPreset::Modern, m);

    CHECK(m.has(Action::Fire, Source::PadButton, 7));
    CHECK(m.has(Action::Special, Source::MouseButton, 1));
    CHECK(m.has(Action::Fire, Source::Key, JK_SPACE));
}
