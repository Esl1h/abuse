/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Unit tests for the screen shake (phase 6, block 6.4).
 *
 *  This software was released into the Public Domain.
 */

#include <doctest/doctest.h>

#include "render/shake.h"

namespace {

struct ShakeReset
{
    ~ShakeReset()
    {
        abuse::render::set_shake_enabled(true);
        abuse::render::reset_shake();
    }
};

}

TEST_CASE("nothing shakes until something knocks it") {
    ShakeReset reset;
    abuse::render::reset_shake();

    CHECK(abuse::render::shake_amount() == doctest::Approx(0.0f));

    int dx = 99, dy = 99;
    abuse::render::shake_offset(dx, dy);
    CHECK(dx == 0);
    CHECK(dy == 0);
}

TEST_CASE("a knock decays to nothing within a fraction of a second") {
    ShakeReset reset;
    abuse::render::reset_shake();

    abuse::render::shake(6.0f);
    CHECK(abuse::render::shake_amount() == doctest::Approx(6.0f));

    // At 15 Hz, ten ticks is two thirds of a second. It must be over well
    // before that, or it stops being an impact and becomes a nuisance.
    int ticks = 0;
    while (abuse::render::shake_amount() > 0.0f && ticks < 100)
    {
        abuse::render::shake_tick();
        ticks++;
    }
    CHECK(ticks > 1);
    CHECK(ticks <= 10);
}

TEST_CASE("knocks add up but stop at the ceiling") {
    ShakeReset reset;
    abuse::render::reset_shake();

    for (int i = 0; i < 20; i++)
        abuse::render::shake(5.0f);

    // Eight game pixels on a 320 by 200 screen is already a lot.
    CHECK(abuse::render::shake_amount() <= 8.0f);

    int dx = 0, dy = 0;
    abuse::render::shake_offset(dx, dy);
    CHECK(dx >= -8);
    CHECK(dx <= 8);
    CHECK(dy >= -8);
    CHECK(dy <= 8);
}

TEST_CASE("the offset moves around rather than sitting still") {
    ShakeReset reset;
    abuse::render::reset_shake();
    abuse::render::shake(8.0f);

    int first_dx = 0, first_dy = 0;
    abuse::render::shake_offset(first_dx, first_dy);

    bool moved = false;
    for (int i = 0; i < 20 && !moved; i++)
    {
        int dx = 0, dy = 0;
        abuse::render::shake_offset(dx, dy);
        if (dx != first_dx || dy != first_dy)
            moved = true;
    }
    CHECK(moved);
}

TEST_CASE("turned off, it does nothing at all") {
    ShakeReset reset;
    abuse::render::reset_shake();
    abuse::render::set_shake_enabled(false);

    abuse::render::shake(8.0f);
    CHECK(abuse::render::shake_amount() == doctest::Approx(0.0f));

    int dx = 1, dy = 1;
    abuse::render::shake_offset(dx, dy);
    CHECK(dx == 0);
    CHECK(dy == 0);
}

TEST_CASE("a knock of nothing is not a knock") {
    ShakeReset reset;
    abuse::render::reset_shake();

    abuse::render::shake(0.0f);
    abuse::render::shake(-3.0f);
    CHECK(abuse::render::shake_amount() == doctest::Approx(0.0f));

    // And ticking an idle shake is harmless.
    abuse::render::shake_tick();
    CHECK(abuse::render::shake_amount() == doctest::Approx(0.0f));
}
