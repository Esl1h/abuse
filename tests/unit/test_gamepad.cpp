#include <doctest/doctest.h>

#include <initializer_list>
#include <string.h>

#include "input/gamepad.h"

using namespace abuse::input;

TEST_CASE("a fresh pad has nothing held") {
    PadState p;
    for (int i = 0; i < PadState::kMaxButtons; i++)
        CHECK_FALSE(p.button(i));
    for (int i = 0; i < PadState::kMaxAxes; i++)
        CHECK(p.axis(i) == 0);
}

TEST_CASE("buttons hold their state") {
    PadState p;
    p.set_button(3, true);
    CHECK(p.button(3));
    CHECK_FALSE(p.button(4));
    p.set_button(3, false);
    CHECK_FALSE(p.button(3));
}

TEST_CASE("out of range indices are ignored, not crashed on") {
    PadState p;
    p.set_button(-1, true);
    p.set_button(PadState::kMaxButtons, true);
    p.set_axis(-1, 30000);
    p.set_axis(PadState::kMaxAxes, 30000);

    CHECK_FALSE(p.button(-1));
    CHECK_FALSE(p.button(PadState::kMaxButtons));
    CHECK(p.axis(-1) == 0);
    CHECK(p.axis(PadState::kMaxAxes) == 0);
}

TEST_CASE("axis values are clamped to the SDL range") {
    PadState p;
    p.set_axis(0, 999999);
    CHECK(p.axis(0) == kAxisMax);
    p.set_axis(0, -999999);
    CHECK(p.axis(0) == kAxisMin);
}

TEST_CASE("inside the deadzone the axis reads as centred") {
    PadState p;
    Deadzone dz;

    p.set_axis(0, 0);
    CHECK(p.axis_scaled(0, dz) == doctest::Approx(0.0f));
    p.set_axis(0, dz.inner - 1);
    CHECK(p.axis_scaled(0, dz) == doctest::Approx(0.0f));
    p.set_axis(0, -(dz.inner - 1));
    CHECK(p.axis_scaled(0, dz) == doctest::Approx(0.0f));
}

TEST_CASE("past the outer edge the axis reads as fully deflected") {
    PadState p;
    Deadzone dz;

    p.set_axis(0, dz.outer);
    CHECK(p.axis_scaled(0, dz) == doctest::Approx(1.0f));
    p.set_axis(0, kAxisMax);
    CHECK(p.axis_scaled(0, dz) == doctest::Approx(1.0f));
    p.set_axis(0, -dz.outer);
    CHECK(p.axis_scaled(0, dz) == doctest::Approx(-1.0f));
}

// Leaving the deadzone must not jump: a stick that snaps from 0 to 0.25 as
// soon as it moves feels broken.
TEST_CASE("the value ramps from zero as the stick leaves the deadzone") {
    PadState p;
    Deadzone dz;

    p.set_axis(0, dz.inner + 1);
    float just_out = p.axis_scaled(0, dz);
    CHECK(just_out > 0.0f);
    CHECK(just_out < 0.01f);

    p.set_axis(0, (dz.inner + dz.outer) / 2);
    CHECK(p.axis_scaled(0, dz) == doctest::Approx(0.5f).epsilon(0.02));
}

TEST_CASE("axis_active answers per direction") {
    PadState p;
    Deadzone dz;

    p.set_axis(0, -30000);
    CHECK(p.axis_active(0, -1, dz));
    CHECK_FALSE(p.axis_active(0, 1, dz));

    p.set_axis(0, 30000);
    CHECK_FALSE(p.axis_active(0, -1, dz));
    CHECK(p.axis_active(0, 1, dz));

    p.set_axis(0, 0);
    CHECK_FALSE(p.axis_active(0, -1, dz));
    CHECK_FALSE(p.axis_active(0, 1, dz));

    CHECK_FALSE(p.axis_active(0, 0, dz));
}

// The bug this module exists to avoid. The event-based path in
// src/sdlport/event.cpp decides which direction to release from the sign of
// the value at release time, so a stick that settles a hair past centre
// releases the direction that was not held and strands the other one.
TEST_CASE("a stick that settles past centre leaves nothing held") {
    PadState p;
    Deadzone dz;

    p.set_axis(0, -32000);              // the player holds left
    CHECK(p.axis_active(0, -1, dz));

    p.set_axis(0, 137);                 // released; rests a hair right of centre
    CHECK_FALSE(p.axis_active(0, -1, dz));
    CHECK_FALSE(p.axis_active(0, 1, dz));
}

TEST_CASE("a disconnect releases everything") {
    PadState p;
    p.set_button(2, true);
    p.set_axis(1, 30000);

    p.disconnect();

    CHECK_FALSE(p.button(2));
    CHECK(p.axis(1) == 0);
}

TEST_CASE("a nonsensical deadzone does not invert the axis") {
    PadState p;
    Deadzone dz;
    dz.inner = 20000;
    dz.outer = 1000;    // smaller than the inner one

    p.set_axis(0, 25000);
    float v = p.axis_scaled(0, dz);
    CHECK(v > 0.0f);
    CHECK(v <= 1.0f);
}

TEST_CASE("a zero inner deadzone still works") {
    PadState p;
    Deadzone dz;
    dz.inner = 0;

    p.set_axis(0, 1);
    CHECK(p.axis_scaled(0, dz) > 0.0f);
    p.set_axis(0, 0);
    CHECK(p.axis_scaled(0, dz) == doctest::Approx(0.0f));
}

// Task 3.3: the trigger has its own, much smaller, deadzone. Sharing the
// stick's made the right trigger need a quarter of its travel before firing.
TEST_CASE("the trigger reacts earlier than the stick at the same deflection") {
    PadState p;

    Deadzone stick;             // 8192 inner, o padrao do analogico
    Deadzone trigger{2000, 30000};

    int const light_press = 4000;   // entre os dois cortes

    p.set_axis(0, light_press);
    CHECK_FALSE(p.axis_active(0, 1, stick));
    CHECK(p.axis_active(0, 1, trigger));
}

TEST_CASE("deadzone_for_axis picks the trigger zone only for triggers") {
    // Stick axes keep the wide cutoff.
    CHECK(deadzone_for_axis(0).inner == deadzone().inner);
    CHECK(deadzone_for_axis(1).inner == deadzone().inner);
    CHECK(deadzone_for_axis(2).inner == deadzone().inner);
    CHECK(deadzone_for_axis(3).inner == deadzone().inner);

    CHECK(deadzone_for_axis(kAxisLeftTrigger).inner == trigger_deadzone().inner);
    CHECK(deadzone_for_axis(kAxisRightTrigger).inner == trigger_deadzone().inner);

    // An axis the pad does not have falls back to the stick zone rather than
    // to something undefined.
    CHECK(deadzone_for_axis(99).inner == deadzone().inner);
}

TEST_CASE("deadzone values parse from the config") {
    int v = -1;

    REQUIRE(parse_deadzone_value("8192", v));
    CHECK(v == 8192);
    REQUIRE(parse_deadzone_value("0", v));
    CHECK(v == 0);
    REQUIRE(parse_deadzone_value("32767", v));
    CHECK(v == 32767);
}

// A typo in abuserc must leave the default alone instead of storing garbage.
TEST_CASE("invalid deadzone values are rejected and leave the target alone") {
    int v = 8192;

    CHECK_FALSE(parse_deadzone_value("-100", v));    // negativo
    CHECK_FALSE(parse_deadzone_value("32768", v));   // acima do maximo do eixo
    CHECK_FALSE(parse_deadzone_value("99999", v));
    CHECK_FALSE(parse_deadzone_value("abc", v));
    CHECK_FALSE(parse_deadzone_value("80x0", v));    // lixo depois do numero
    CHECK_FALSE(parse_deadzone_value("", v));
    CHECK_FALSE(parse_deadzone_value(nullptr, v));

    CHECK(v == 8192);
}

// The same physical button carries a different label on each family, and the
// Nintendo layout swaps A/B against Xbox. Telling a Switch player to press A
// when the game means the south button sends them to the wrong button.
TEST_CASE("face button labels follow the family") {
    CHECK(strcmp(button_label(PadFamily::Xbox, 0), "A") == 0);
    CHECK(strcmp(button_label(PadFamily::Xbox, 1), "B") == 0);

    CHECK(strcmp(button_label(PadFamily::Nintendo, 0), "B") == 0);
    CHECK(strcmp(button_label(PadFamily::Nintendo, 1), "A") == 0);

    CHECK(strcmp(button_label(PadFamily::PlayStation, 0), "Cross") == 0);
    CHECK(strcmp(button_label(PadFamily::PlayStation, 3), "Triangle") == 0);

    CHECK(strcmp(button_label(PadFamily::Generic, 0), "1") == 0);
}

TEST_CASE("the d-pad reads the same on every family") {
    for (PadFamily f : { PadFamily::Generic, PadFamily::Xbox,
                         PadFamily::PlayStation, PadFamily::Nintendo })
    {
        CHECK(strcmp(button_label(f, 11), "Up") == 0);
        CHECK(strcmp(button_label(f, 14), "Right") == 0);
    }
}

TEST_CASE("a button with no label returns null instead of inventing one") {
    CHECK(button_label(PadFamily::Xbox, 99) == nullptr);
    CHECK(button_label(PadFamily::Xbox, -1) == nullptr);
    CHECK(button_label(PadFamily::Xbox, 7) == nullptr);   // start, which has no glyph
}

TEST_CASE("family names parse and round trip") {
    PadFamily f = PadFamily::Generic;
    for (char const *name : { "xbox", "playstation", "nintendo", "generic" })
    {
        REQUIRE(parse_family(name, f));
        CHECK(strcmp(family_name(f), name) == 0);
    }

    CHECK_FALSE(parse_family("sega", f));
    CHECK_FALSE(parse_family(nullptr, f));
}


// The menu cursor moves once per tick from the stick position. Driving it from
// axis events instead left it standing still whenever the stick was held, which
// is what a player does when crossing a menu.
TEST_CASE("the menu cursor moves further the harder the stick is pushed") {
    CursorStep half = menu_cursor_step(0.5f, 0.0f, 16);
    CursorStep full = menu_cursor_step(1.0f, 0.0f, 16);
    CHECK(full.x > half.x);
    CHECK(half.x > 0);
    CHECK(full.y == 0);
}

TEST_CASE("a full push covers the speed it was given") {
    CursorStep step = menu_cursor_step(1.0f, 0.0f, 16);
    CHECK(step.x == 16);
    CHECK(menu_cursor_step(-1.0f, 0.0f, 16).x == -16);
    CHECK(menu_cursor_step(0.0f, 1.0f, 16).y == 16);
    CHECK(menu_cursor_step(0.0f, -1.0f, 16).y == -16);
}

// The squared response is what makes a slot list selectable: near the deadzone
// the cursor has to crawl, not jump.
TEST_CASE("a small push moves slowly but never stands still") {
    for (float v = 0.05f; v < 0.3f; v += 0.05f)
    {
        CursorStep step = menu_cursor_step(v, 0.0f, 16);
        CHECK(step.x >= 1);
        CHECK(step.x <= 3);
    }
}

TEST_CASE("a diagonal is no faster than a cardinal") {
    CursorStep diag = menu_cursor_step(1.0f, 1.0f, 16);
    int len2 = diag.x * diag.x + diag.y * diag.y;
    CHECK(len2 <= 16 * 16 + 1);
}

TEST_CASE("a centred stick does not move the cursor") {
    CursorStep step = menu_cursor_step(0.0f, 0.0f, 16);
    CHECK(step.x == 0);
    CHECK(step.y == 0);
}

TEST_CASE("a speed of zero or less holds the cursor still") {
    CHECK(menu_cursor_step(1.0f, 1.0f, 0).x == 0);
    CHECK(menu_cursor_step(1.0f, 1.0f, -5).y == 0);
}
