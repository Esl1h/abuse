/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See voices.h.
 *
 *  This software was released into the Public Domain.
 */

#include "voices.h"

namespace abuse::audio {

void VoicePool::reset(int count)
{
    if (count < 0)
        count = 0;
    m_slots.assign((size_t)count, Slot());
}

int VoicePool::acquire(int priority, uint32_t now)
{
    if (m_slots.empty())
        return kNone;

    // A free voice, always, before taking one from anybody.
    for (size_t i = 0; i < m_slots.size(); i++)
        if (!m_slots[i].busy)
        {
            m_slots[i].busy = true;
            m_slots[i].priority = priority;
            m_slots[i].started = now;
            return (int)i;
        }

    // Otherwise the weakest, and the oldest among equals: an older sound has
    // already been heard, a newer one has not.
    size_t weakest = 0;
    for (size_t i = 1; i < m_slots.size(); i++)
    {
        if (m_slots[i].priority < m_slots[weakest].priority
            || (m_slots[i].priority == m_slots[weakest].priority
                && m_slots[i].started < m_slots[weakest].started))
            weakest = i;
    }

    // Strictly lower: equal priority keeps what is already sounding, which is
    // what stops a burst of identical shots from cutting each other off.
    if (m_slots[weakest].priority >= priority)
        return kNone;

    m_slots[weakest].busy = true;
    m_slots[weakest].priority = priority;
    m_slots[weakest].started = now;
    return (int)weakest;
}

void VoicePool::release(int slot)
{
    if (slot >= 0 && (size_t)slot < m_slots.size())
        m_slots[(size_t)slot] = Slot();
}

void VoicePool::release_all()
{
    for (size_t i = 0; i < m_slots.size(); i++)
        m_slots[i] = Slot();
}

bool VoicePool::busy(int slot) const
{
    if (slot < 0 || (size_t)slot >= m_slots.size())
        return false;
    return m_slots[(size_t)slot].busy;
}

int VoicePool::priority_of(int slot) const
{
    if (slot < 0 || (size_t)slot >= m_slots.size())
        return 0;
    return m_slots[(size_t)slot].priority;
}

}
