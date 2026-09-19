/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Fixed logical tick, decoupled from the render frame.
 *  Phase 2, task 2.4.
 *
 *  Today one iteration of the main loop is one tick AND one frame, held at
 *  15 Hz by Game::calc_speed(). That ties how often the world advances to how
 *  often it is drawn, which is what stops the render from running smoothly and
 *  what phase 7 needs undone before it can interpolate.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_TIMING_PACER_H_
#define ABUSE_TIMING_PACER_H_

namespace abuse::timing {

// The rate the game has always run at. Game logic depends on it: speeds,
// timers and AI counters are all expressed in ticks.
constexpr double kTickHz = 15.0;
constexpr double kTickMs = 1000.0 / kTickHz;

class Pacer
{
public:
    // A stall longer than this many ticks is given up on rather than chased.
    // Without a cap, a machine that falls behind asks for more ticks, which
    // makes it fall further behind: the frame it owes grows without bound.
    static constexpr int kMaxCatchUp = 5;

    explicit Pacer(double tick_ms = kTickMs);

    // Feeds the wall time since the previous call and answers how many logical
    // ticks are owed. Zero is a normal answer: it means draw again without
    // advancing the world.
    int advance(double elapsed_ms);

    // Where the next frame sits between the last tick and the next, in [0,1).
    // Phase 7 uses it to interpolate positions; nothing reads it yet.
    float alpha() const;

    // Drops the debt. Use after a level load or anything else that blocks for
    // a long time, so the pause is not chased.
    void reset();

    // True when the last advance() had to drop ticks it could not catch up on.
    bool dropped() const { return m_dropped; }

private:
    double m_tick_ms;
    double m_accumulator = 0.0;
    bool m_dropped = false;
};

}

#endif
