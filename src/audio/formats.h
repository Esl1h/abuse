/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Which file to actually open for a sound. Phase 5, task 5.1.
 *
 *  The Lisp asks for sounds by name, and every one of those names ends in
 *  .wav because that is what shipped in 1995. A free sound pack cannot be
 *  WAV: the repository has no Git LFS (GitHub refuses new LFS objects on a
 *  public fork), so the pack has to fit in plain git, and that means Vorbis
 *  or FLAC. Renaming what the Lisp asks for is not an option either, because
 *  the engine and the Lisp are coupled by those names.
 *
 *  So the name stays and the lookup widens: ask for foo.wav, and if it is
 *  not there, try foo.ogg and foo.flac. The mixer decodes all three.
 *
 *  Never in the Original mode. That mode plays the files it was given,
 *  exactly as they are, and quietly picking a different file would be the
 *  one thing it exists not to do.
 *
 *  Free of SDL, so the rule can be tested without a mixer.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_AUDIO_FORMATS_H_
#define ABUSE_AUDIO_FORMATS_H_

#include <string>
#include <vector>

namespace abuse::audio {

// The names to try for `requested`, best first. The requested name always
// comes first, so a pack that does ship the exact file wins and nothing has
// to know about this.
//
// With `substitutes` false the list is just the requested name, which is
// what the Original mode passes.
std::vector<std::string> sound_candidates(std::string const &requested,
                                          bool substitutes);

// The extensions tried, in order, after the requested name. Exposed for the
// tests and for anything that wants to report what it looked for.
std::vector<char const *> const &sound_extensions();

}

#endif
