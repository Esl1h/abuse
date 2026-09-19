/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Mix buses. Phase 5, task 5.1.
 *
 *  The engine has always had two volumes, sfx and music, as integers from 0
 *  to 127 that the caller multiplies into each play() itself. They are session
 *  values: nothing writes them down, so every launch starts at full.
 *
 *  This is the mix stage under that: a gain per bus and a master over all of
 *  them, in the range every audio API actually wants, written to abuserc and
 *  read back. Defaults are 1, so a build that never touches them sounds
 *  exactly as it did.
 *
 *  Free of SDL, so the maths can be tested without a device.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_AUDIO_BUSES_H_
#define ABUSE_AUDIO_BUSES_H_

namespace abuse::audio {

enum class Bus
{
    Sfx,        // the world: weapons, doors, voices
    Music,      // the score
    Ui,         // menus and the screens, which must stay audible
    Count
};

int const kBusCount = (int)Bus::Count;

// 0 silences, 1 is unchanged. Above 1 is allowed up to 2, because a free
// sound set assembled from several sources will need some of it lifted; the
// limiter is what keeps that from clipping.
void set_gain(Bus b, float gain);
float gain(Bus b);

void set_master(float gain);
float master();

// What a voice should actually play at: the caller's own volume, 0 to 255 in
// the engine's scale, folded with the bus and the master. Never negative,
// never above 1, because a gain above 1 belongs in the mix and not in a
// single voice.
float voice_gain(Bus b, int volume);

// Names as written in abuserc: volume_sfx, volume_music, volume_ui.
char const *bus_name(Bus b);
bool parse_bus(char const *name, Bus &out);

// "0" to "100", the scale a person reads. Out of range is refused and leaves
// the value alone, like every other parser in the project.
bool parse_percent(char const *text, float &gain);
int percent(float gain);

}

#endif
