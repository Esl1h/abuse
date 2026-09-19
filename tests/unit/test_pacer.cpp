#include <doctest/doctest.h>

#include "timing/pacer.h"

using namespace abuse::timing;

TEST_CASE("the tick rate is the one the game logic assumes") {
    // Speeds, timers and AI counters in data/lisp are expressed in ticks at
    // this rate. Changing it changes the game, not just the smoothness.
    CHECK(kTickHz == doctest::Approx(15.0));
    CHECK(kTickMs == doctest::Approx(66.666).epsilon(0.001));
}

TEST_CASE("exactly one tick of elapsed time owes one tick") {
    Pacer p;
    CHECK(p.advance(kTickMs) == 1);
    CHECK(p.alpha() == doctest::Approx(0.0f));
}

TEST_CASE("a fast frame owes nothing and only moves alpha") {
    Pacer p;
    // 60 Hz display, 15 Hz logic: three frames out of four draw without
    // advancing the world.
    CHECK(p.advance(16.6) == 0);
    CHECK(p.alpha() == doctest::Approx(16.6f / kTickMs).epsilon(0.01));
    CHECK(p.advance(16.6) == 0);
    CHECK(p.advance(16.6) == 0);
    // 4 x 16,6 = 66,4 ms, ainda abaixo de um tick de 66,67 ms.
    CHECK(p.advance(16.6) == 0);
    CHECK(p.advance(16.6) == 1);
}

TEST_CASE("leftover time carries into the next call") {
    Pacer p;
    CHECK(p.advance(kTickMs * 1.5) == 1);
    CHECK(p.alpha() == doctest::Approx(0.5f).epsilon(0.01));
    // Half a tick was owed; half a tick more completes it.
    CHECK(p.advance(kTickMs * 0.5) == 1);
    CHECK(p.alpha() == doctest::Approx(0.0f).epsilon(0.01));
}

TEST_CASE("a slow frame owes several ticks") {
    Pacer p;
    CHECK(p.advance(kTickMs * 3) == 3);
    CHECK_FALSE(p.dropped());
}

// Feeding exactly one tick has to answer exactly one tick, every time. Without
// slack in the comparison the value can land a hair under after a couple of
// multiplications, and the game would run at half speed on a display whose
// refresh divides evenly into 15 Hz.
TEST_CASE("exact tick feeds do not drift") {
    Pacer p;
    int total = 0;
    for (int i = 0; i < 100; i++)
        total += p.advance(kTickMs);
    CHECK(total == 100);
}

// Without a cap, a machine that falls behind asks for more ticks, which makes
// it fall further behind. The debt has to be forgiven at some point.
TEST_CASE("catch-up is capped and the rest is dropped") {
    Pacer p;
    int ticks = p.advance(kTickMs * 100);
    CHECK(ticks == Pacer::kMaxCatchUp);
    CHECK(p.dropped());

    // After dropping, the next frame starts clean instead of still owing 95.
    CHECK(p.advance(0.0) == 0);
    CHECK_FALSE(p.dropped());
    CHECK(p.alpha() == doctest::Approx(0.0f));
}

TEST_CASE("a clock that goes backwards owes nothing") {
    Pacer p;
    CHECK(p.advance(-1000.0) == 0);
    CHECK(p.alpha() == doctest::Approx(0.0f));
    CHECK(p.advance(0.0) == 0);
}

TEST_CASE("reset forgets the debt") {
    Pacer p;
    p.advance(kTickMs * 0.9);
    CHECK(p.alpha() > 0.5f);
    p.reset();
    CHECK(p.alpha() == doctest::Approx(0.0f));
    // A level load takes seconds; that pause must not be chased afterwards.
    CHECK(p.advance(kTickMs * 0.5) == 0);
}

TEST_CASE("alpha stays inside [0,1)") {
    Pacer p;
    double const feeds[] = { 1.0, 5.0, 33.3, 66.0, 70.0, 200.0, 0.1 };
    for (double ms : feeds)
    {
        p.advance(ms);
        CHECK(p.alpha() >= 0.0f);
        CHECK(p.alpha() < 1.0f);
    }
}

// Over a long run the number of ticks must track wall time, or the game would
// drift slow or fast against the clock.
TEST_CASE("tick count tracks wall time over a long run") {
    Pacer p;
    int total = 0;
    // 600 frames at 60 Hz is 10 seconds; 15 Hz logic owes 150 ticks.
    for (int i = 0; i < 600; i++)
        total += p.advance(1000.0 / 60.0);

    CHECK(total == 150);
}
