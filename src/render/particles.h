/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Particles: sparks, smoke and ejected casings. Phase 6, block 6.4.
 *
 *  Decoration only. Particles live in world coordinates and advance once per
 *  logical tick, like everything else that moves, but nothing in the game
 *  can see them: they collide with nothing, they are not objects, and they
 *  take no numbers from the game's RNG. A replay recorded with them on
 *  hashes the same as one recorded with them off, which is the whole reason
 *  the state lives here rather than in the object list.
 *
 *  Free of SDL and of imlib, so the motion can be tested without a window.
 *  Which palette entry a particle ends up as is the drawer's problem; this
 *  side only says what kind of particle it is and how far through its life.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_RENDER_PARTICLES_H_
#define ABUSE_RENDER_PARTICLES_H_

namespace abuse::render {

enum class ParticleKind
{
    Spark,      // hot, thrown out fast, falls and cools
    Smoke,      // rises, drifts, thins out
    Casing      // ejected sideways, falls, does not fade
};

// One particle as the drawer sees it. The position is in world pixels, the
// same space objects are in, so the drawer subtracts the view offset the way
// it does for everything else.
struct ParticleView
{
    int x;
    int y;
    ParticleKind kind;

    // 0 at birth, 1 at death. The drawer turns this into a colour.
    float age;
};

// Spawns. `count` is a request, not a promise: the pool has a ceiling, and a
// burst that does not fit is silently shortened rather than allowed to push
// out the particles already in the air.
//
// `dir` is -1 or 1 and says which way the spray leans; 0 spreads evenly.
void spawn_sparks(int x, int y, int count, int dir);
void spawn_smoke(int x, int y, int count);
void spawn_casing(int x, int y, int dir);

// Advances every particle by one logical tick and retires the dead ones.
void particles_tick();

// After a level load or anything else that should not inherit the last
// frame's debris.
void reset_particles();

// Iteration, for the drawer.
int particle_count();
ParticleView particle_at(int i);

// The setting. Off means the spawns do nothing at all, so there is no cost
// beyond the call itself.
bool particles_enabled();
void set_particles_enabled(bool on);

}

#endif
