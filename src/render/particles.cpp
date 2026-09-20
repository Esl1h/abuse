/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See particles.h.
 *
 *  This software was released into the Public Domain.
 */

#include "particles.h"

#include "options.h"

#include <stdint.h>

namespace abuse::render {

namespace {

// Small on purpose. The game draws at 320x200 and a burst of debris reads as
// a mess long before it reads as a lot, so the ceiling is there to keep the
// picture legible rather than to save memory.
int const kMax = 256;

// Per tick, in pixels. The game's own gravity is a Lisp constant and this
// does not read it: debris that falls at exactly the rate a body falls looks
// heavier than it should, and matching it would tie decoration to logic.
float const kGravity = 0.45f;
float const kDrag = 0.88f;

struct Particle
{
    float x, y;
    float vx, vy;
    int life;
    int born;
    ParticleKind kind;
};

Particle g_pool[kMax];
int g_live = 0;
bool g_enabled = true;

// Its own sequence, for the same reason the shake has one: taking numbers
// from the game's RNG here would move every roll the simulation makes
// afterwards and the replays would stop matching.
uint32_t g_seed = 0x2545f491u;

uint32_t next_random()
{
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}

// A number in [-1, 1].
float signed_unit()
{
    return (float)(int32_t)(next_random() % 2001u) / 1000.0f - 1.0f;
}

// A number in [0, 1].
float unit()
{
    return (float)(next_random() % 1001u) / 1000.0f;
}

Particle *take()
{
    if (g_live >= kMax)
        return nullptr;
    return &g_pool[g_live++];
}

}

void spawn_sparks(int x, int y, int count, int dir)
{
    if (!g_enabled || !motion_allowed())
        return;

    for (int i = 0; i < count; i++)
    {
        Particle *p = take();
        if (!p)
            return;

        p->x = (float)x;
        p->y = (float)y;

        // Thrown out and slightly up, so they arc instead of raining
        // straight down.
        float const spread = dir == 0 ? signed_unit() : (float)dir * unit();
        p->vx = spread * 3.0f;
        p->vy = -unit() * 2.0f;

        p->life = 4 + (int)(next_random() % 5u);
        p->born = p->life;
        p->kind = ParticleKind::Spark;
    }
}

void spawn_smoke(int x, int y, int count)
{
    if (!g_enabled || !motion_allowed())
        return;

    for (int i = 0; i < count; i++)
    {
        Particle *p = take();
        if (!p)
            return;

        p->x = (float)x + signed_unit() * 2.0f;
        p->y = (float)y;
        p->vx = signed_unit() * 0.4f;
        p->vy = -0.5f - unit() * 0.4f;

        p->life = 8 + (int)(next_random() % 7u);
        p->born = p->life;
        p->kind = ParticleKind::Smoke;
    }
}

void spawn_casing(int x, int y, int dir)
{
    if (!g_enabled || !motion_allowed())
        return;

    Particle *p = take();
    if (!p)
        return;

    p->x = (float)x;
    p->y = (float)y;

    // Out of the side of the gun, away from where the shot went.
    p->vx = (dir == 0 ? 1.0f : (float)-dir) * (0.8f + unit() * 0.6f);
    p->vy = -1.2f - unit() * 0.5f;

    p->life = 10 + (int)(next_random() % 6u);
    p->born = p->life;
    p->kind = ParticleKind::Casing;
}

void particles_tick()
{
    int kept = 0;
    for (int i = 0; i < g_live; i++)
    {
        Particle &p = g_pool[i];

        p.life--;
        if (p.life <= 0)
            continue;

        p.x += p.vx;
        p.y += p.vy;

        switch (p.kind)
        {
        case ParticleKind::Spark:
            p.vy += kGravity;
            p.vx *= kDrag;
            break;
        case ParticleKind::Smoke:
            // Rises and slows, and wanders a little on the way.
            p.vy *= 0.9f;
            p.vx = p.vx * 0.95f + signed_unit() * 0.08f;
            break;
        case ParticleKind::Casing:
            p.vy += kGravity;
            break;
        }

        if (kept != i)
            g_pool[kept] = p;
        kept++;
    }
    g_live = kept;
}

void reset_particles()
{
    g_live = 0;
}

int particle_count()
{
    return g_live;
}

ParticleView particle_at(int i)
{
    ParticleView v = { 0, 0, ParticleKind::Spark, 1.0f };
    if (i < 0 || i >= g_live)
        return v;

    Particle const &p = g_pool[i];
    v.x = (int)p.x;
    v.y = (int)p.y;
    v.kind = p.kind;
    v.age = p.born > 0 ? 1.0f - (float)p.life / (float)p.born : 1.0f;
    return v;
}

bool particles_enabled()
{
    return g_enabled;
}

void set_particles_enabled(bool on)
{
    g_enabled = on;
    if (!on)
        reset_particles();
}

}
