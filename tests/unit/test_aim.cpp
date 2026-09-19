#include <doctest/doctest.h>

#include <initializer_list>
#include <math.h>

#include "compat.h"
#include "input/aim.h"

using namespace abuse::input;

TEST_CASE("a centred stick gives no offset") {
    AimVector v = aim_offset_from_axes(0.0f, 0.0f, 80);
    CHECK(v.x == 0);
    CHECK(v.y == 0);
}

TEST_CASE("full deflection reaches the radius") {
    CHECK(aim_offset_from_axes(1.0f, 0.0f, 80).x == 80);
    CHECK(aim_offset_from_axes(-1.0f, 0.0f, 80).x == -80);
    CHECK(aim_offset_from_axes(0.0f, 1.0f, 80).y == 80);
    CHECK(aim_offset_from_axes(0.0f, -1.0f, 80).y == -80);
}

TEST_CASE("half deflection reaches half the radius") {
    AimVector v = aim_offset_from_axes(0.5f, 0.0f, 80);
    CHECK(v.x == 40);
    CHECK(v.y == 0);
}

// The axes describe a square, but the crosshair should travel on a circle:
// otherwise a diagonal reaches about 40% further than a cardinal, and the
// player can hit things diagonally that are out of range straight ahead.
TEST_CASE("diagonals are clamped to the circle, not the square") {
    AimVector v = aim_offset_from_axes(1.0f, 1.0f, 100);

    double len = (double)v.x * v.x + (double)v.y * v.y;
    CHECK(len <= 100.0 * 100.0 + 1.0);

    // And it really is diagonal, not flattened onto an axis.
    CHECK(v.x > 60);
    CHECK(v.y > 60);
    CHECK(v.x == v.y);
}

TEST_CASE("a partial diagonal inside the circle is left alone") {
    AimVector v = aim_offset_from_axes(0.5f, 0.5f, 100);
    // 0.5,0.5 has length 0.707, inside the circle, so it scales directly.
    CHECK(v.x == 50);
    CHECK(v.y == 50);
}

TEST_CASE("the radius scales the reach") {
    CHECK(aim_offset_from_axes(1.0f, 0.0f, 32).x == 32);
    CHECK(aim_offset_from_axes(1.0f, 0.0f, 160).x == 160);
}

TEST_CASE("a nonsensical radius does not produce a nonsensical offset") {
    CHECK(aim_offset_from_axes(1.0f, 0.0f, 0).x >= 0);
    CHECK(aim_offset_from_axes(1.0f, 0.0f, -50).x >= 0);
}

TEST_CASE("the pad aim flag can be handed back to the mouse") {
    set_pad_aim_active(true);
    CHECK(pad_aim_active());
    set_pad_aim_active(false);
    CHECK_FALSE(pad_aim_active());
}

namespace {

double length_of(AimVector const &v)
{
    return sqrt((double)v.x * v.x + (double)v.y * v.y);
}

// Angle between two vectors, in degrees, for readable assertions.
double angle_between(double ax, double ay, double bx, double by)
{
    double la = sqrt(ax * ax + ay * ay), lb = sqrt(bx * bx + by * by);
    double dot = (ax * bx + ay * by) / (la * lb);
    if (dot > 1.0) dot = 1.0;
    if (dot < -1.0) dot = -1.0;
    return acos(dot) * 180.0 / abuse::kPi;
}

}

// Off by default: helping the player aim changes how the game plays, so it has
// to be asked for, and the Original mode never gets it.
TEST_CASE("assist is off by default and then does nothing") {
    AssistSettings s;
    CHECK(s.strength == 0);

    AimVector aim{80, 0};
    AimTarget t{60, 60};
    AimVector out = assist_aim(aim, &t, 1, s);
    CHECK(out.x == aim.x);
    CHECK(out.y == aim.y);
}

TEST_CASE("with no candidates the aim is untouched") {
    AssistSettings s; s.strength = 50;
    AimVector aim{80, 0};
    AimVector out = assist_aim(aim, nullptr, 0, s);
    CHECK(out.x == 80);
    CHECK(out.y == 0);
}

TEST_CASE("a candidate inside the cone pulls the aim towards it") {
    AssistSettings s; s.strength = 50; s.cone_degrees = 30;
    AimVector aim{80, 0};                 // pointing right
    AimTarget t{100, 40};                 // cerca de 21 graus abaixo

    AimVector out = assist_aim(aim, &t, 1, s);

    double before = angle_between(aim.x, aim.y, t.dx, t.dy);
    double after = angle_between(out.x, out.y, t.dx, t.dy);
    CHECK(after < before);
    CHECK(after > 0.0);                   // 50% pulls without snapping
}

// Outside the cone the aim must not move at all: otherwise the crosshair jumps
// across the screen to something the player was not pointing at.
TEST_CASE("a candidate outside the cone is ignored") {
    AssistSettings s; s.strength = 100; s.cone_degrees = 20;
    AimVector aim{80, 0};
    AimTarget t{0, 80};                   // 90 graus

    AimVector out = assist_aim(aim, &t, 1, s);
    CHECK(out.x == 80);
    CHECK(out.y == 0);
}

TEST_CASE("full strength snaps onto the candidate direction") {
    AssistSettings s; s.strength = 100; s.cone_degrees = 45;
    AimVector aim{80, 0};
    AimTarget t{100, 60};

    AimVector out = assist_aim(aim, &t, 1, s);
    CHECK(angle_between(out.x, out.y, t.dx, t.dy) < 1.0);
}

// The assist changes where the player points, never how far: the reach is the
// aimradius setting and must survive untouched.
TEST_CASE("the reach is preserved whatever the assist does") {
    AssistSettings s; s.cone_degrees = 60;
    AimVector aim{80, 0};
    AimTarget t{30, 50};

    for (int strength : { 10, 50, 100 })
    {
        s.strength = strength;
        AimVector out = assist_aim(aim, &t, 1, s);
        CHECK(length_of(out) == doctest::Approx(length_of(aim)).epsilon(0.02));
    }
}

TEST_CASE("the closest to the aim wins among several candidates") {
    AssistSettings s; s.strength = 100; s.cone_degrees = 60;
    AimVector aim{80, 0};

    AimTarget targets[3];
    targets[0] = AimTarget{60, 50};    // longe do eixo
    targets[1] = AimTarget{90, 5};     // quase alinhado
    targets[2] = AimTarget{40, 35};    // intermediario

    AimVector out = assist_aim(aim, targets, 3, s);
    CHECK(angle_between(out.x, out.y, targets[1].dx, targets[1].dy) < 1.0);
}

TEST_CASE("a candidate on top of the player is skipped") {
    AssistSettings s; s.strength = 100; s.cone_degrees = 60;
    AimVector aim{80, 0};
    AimTarget t{0, 0};                    // no direction

    AimVector out = assist_aim(aim, &t, 1, s);
    CHECK(out.x == 80);
    CHECK(out.y == 0);
}

TEST_CASE("an aim with no direction is left alone") {
    AssistSettings s; s.strength = 100;
    AimVector aim{0, 0};
    AimTarget t{80, 0};

    AimVector out = assist_aim(aim, &t, 1, s);
    CHECK(out.x == 0);
    CHECK(out.y == 0);
}
