/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Unit tests for the particles (phase 6, block 6.4).
 *
 *  This software was released into the Public Domain.
 */

#include <doctest/doctest.h>

#include "render/options.h"
#include "render/particles.h"

using namespace abuse::render;

namespace {

struct ParticleReset
{
    ~ParticleReset()
    {
        set_particles_enabled(true);
        options().reduce_motion = false;
        reset_particles();
    }
};

// Runs the world forward until nothing is left, with a ceiling so a bug in
// the retirement cannot hang the suite.
int ticks_until_empty(int limit = 1000)
{
    for (int i = 0; i < limit; i++)
    {
        if (particle_count() == 0)
            return i;
        particles_tick();
    }
    return limit;
}

}

TEST_CASE("nothing is in the air until something spawns") {
    ParticleReset reset;
    reset_particles();

    CHECK(particle_count() == 0);
    particles_tick();
    CHECK(particle_count() == 0);
}

TEST_CASE("a burst spawns what was asked for") {
    ParticleReset reset;
    reset_particles();

    spawn_sparks(100, 50, 6, 1);
    CHECK(particle_count() == 6);

    spawn_casing(100, 50, 1);
    CHECK(particle_count() == 7);

    spawn_smoke(100, 50, 3);
    CHECK(particle_count() == 10);
}

TEST_CASE("every particle dies, and the pool empties") {
    ParticleReset reset;
    reset_particles();

    spawn_sparks(0, 0, 8, 0);
    spawn_smoke(0, 0, 8);
    spawn_casing(0, 0, -1);

    int const ticks = ticks_until_empty();
    CHECK(ticks > 0);
    CHECK(ticks < 100);
    CHECK(particle_count() == 0);
}

TEST_CASE("the pool has a ceiling and keeps what is already flying") {
    ParticleReset reset;
    reset_particles();

    spawn_sparks(10, 10, 4, 1);
    int const first = particle_count();
    CHECK(first == 4);

    // Far more than fits. The request is shortened; it does not wrap, and it
    // does not evict.
    spawn_sparks(20, 20, 5000, 1);
    int const full = particle_count();
    CHECK(full > first);
    CHECK(full <= 256);

    // The four from before are still the first four, untouched.
    for (int i = 0; i < first; i++)
    {
        ParticleView p = particle_at(i);
        CHECK(p.age == doctest::Approx(0.0f));
    }
}

TEST_CASE("a casing falls and smoke rises") {
    ParticleReset reset;
    reset_particles();

    // Both start by going up. Only gravity tells them apart, and it takes a
    // few ticks to win, so this looks at the end of the life and not at the
    // start of it. A spark is left out on purpose: it lives four to eight
    // ticks and can die while still on the way up, which is what a spark
    // does and not something to assert against.
    spawn_casing(0, 100, 1);
    int const casing_start = particle_at(0).y;
    int casing_last = casing_start;
    while (particle_count() > 0)
    {
        casing_last = particle_at(0).y;
        particles_tick();
    }
    CHECK(casing_last > casing_start);

    reset_particles();
    spawn_smoke(0, 100, 1);
    int const smoke_start = particle_at(0).y;
    for (int i = 0; i < 3; i++)
        particles_tick();
    REQUIRE(particle_count() == 1);
    CHECK(particle_at(0).y < smoke_start);
}

TEST_CASE("sparks lean the way they were thrown") {
    ParticleReset reset;
    reset_particles();

    spawn_sparks(100, 50, 20, 1);
    for (int i = 0; i < 2; i++)
        particles_tick();

    int moved = 0;
    for (int i = 0; i < particle_count(); i++)
    {
        CHECK(particle_at(i).x >= 100);
        if (particle_at(i).x > 100)
            moved++;
    }
    CHECK(moved > 0);

    reset_particles();
    spawn_sparks(100, 50, 20, -1);
    for (int i = 0; i < 2; i++)
        particles_tick();

    for (int i = 0; i < particle_count(); i++)
        CHECK(particle_at(i).x <= 100);
}

TEST_CASE("age runs from nothing to everything") {
    ParticleReset reset;
    reset_particles();

    spawn_casing(0, 0, 1);
    CHECK(particle_at(0).age == doctest::Approx(0.0f));

    float last = 0.0f;
    while (particle_count() > 0)
    {
        float const now = particle_at(0).age;
        CHECK(now >= last);
        CHECK(now <= 1.0f);
        last = now;
        particles_tick();
    }
    CHECK(last > 0.5f);
}

TEST_CASE("reading past the end is harmless") {
    ParticleReset reset;
    reset_particles();

    spawn_sparks(5, 5, 2, 1);
    ParticleView p = particle_at(99);
    CHECK(p.x == 0);
    CHECK(p.y == 0);
    p = particle_at(-1);
    CHECK(p.x == 0);
}

TEST_CASE("turning particles off clears the air") {
    ParticleReset reset;
    reset_particles();

    spawn_sparks(0, 0, 10, 1);
    CHECK(particle_count() == 10);

    set_particles_enabled(false);
    CHECK(particle_count() == 0);

    spawn_sparks(0, 0, 10, 1);
    CHECK(particle_count() == 0);
}

TEST_CASE("reduce motion vetoes the spawns") {
    ParticleReset reset;
    reset_particles();
    set_particles_enabled(true);
    options().reduce_motion = true;

    spawn_sparks(0, 0, 10, 1);
    spawn_smoke(0, 0, 10);
    spawn_casing(0, 0, 1);
    CHECK(particle_count() == 0);

    // And the setting itself is untouched, so it comes back.
    options().reduce_motion = false;
    CHECK(particles_enabled());
    spawn_sparks(0, 0, 3, 1);
    CHECK(particle_count() == 3);
}
