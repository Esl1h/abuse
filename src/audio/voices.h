/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Which voice a sound gets, and what happens when they are all busy.
 *  Phase 5, task 5.1.
 *
 *  Today the backend walks its tracks and takes the first one that is not
 *  playing; when none is free the sound is dropped, whatever it was. In a
 *  firefight that means the shot that killed the player can be the one that
 *  goes missing, because an ambient loop got there first.
 *
 *  This decides instead: a free voice if there is one, otherwise the weakest
 *  voice playing, and only if the newcomer outranks it. Free of SDL, so the
 *  policy can be tested without a device.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_AUDIO_VOICES_H_
#define ABUSE_AUDIO_VOICES_H_

#include <stdint.h>

#include <vector>

namespace abuse::audio {

// Higher wins. Coarse on purpose: the point is that a door does not silence
// a death, not that every sound has a rank of its own.
enum Priority
{
    kAmbient  = 0,      // loops and background
    kNormal   = 10,     // most of the world
    kImportant = 20,    // the player: pain, death, weapons
    kUi       = 30      // menus, which must always be heard
};

class VoicePool
{
public:
    // -1 when every voice is busy with something at least as important.
    // constexpr, so taking its address in a CHECK does not need a definition
    // in the .cpp under C++17.
    static constexpr int kNone = -1;

    // Sized to the number of tracks the backend managed to create.
    void reset(int count);
    int size() const { return (int)m_slots.size(); }

    // `now` is any monotonic millisecond count; it only orders voices against
    // each other, so where it comes from does not matter.
    int acquire(int priority, uint32_t now);

    // The backend tells it what actually finished, since only the backend can
    // see a track stop on its own.
    void release(int slot);
    void release_all();

    bool busy(int slot) const;
    int priority_of(int slot) const;

private:
    struct Slot
    {
        bool busy = false;
        int priority = 0;
        uint32_t started = 0;
    };

    std::vector<Slot> m_slots;
};

}

#endif
