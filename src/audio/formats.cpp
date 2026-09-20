/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See formats.h.
 *
 *  This software was released into the Public Domain.
 */

#include "formats.h"

#include <strings.h>

namespace abuse::audio {

namespace {

// Vorbis before FLAC because the pack is meant to fit in plain git and
// Vorbis is the smaller of the two; WAV last, for a pack that ships it.
char const *const kExtensions[] = { ".ogg", ".flac", ".wav" };

std::vector<char const *> build_extensions()
{
    std::vector<char const *> v;
    for (char const *e : kExtensions)
        v.push_back(e);
    return v;
}

// Where the extension starts, or npos. Only looks after the last separator,
// so a directory with a dot in it is not mistaken for one.
size_t extension_at(std::string const &path)
{
    size_t const slash = path.find_last_of("/\\");
    size_t const dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return std::string::npos;
    if (slash != std::string::npos && dot < slash)
        return std::string::npos;
    return dot;
}

}

std::vector<char const *> const &sound_extensions()
{
    static std::vector<char const *> const v = build_extensions();
    return v;
}

std::vector<std::string> sound_candidates(std::string const &requested,
                                          bool substitutes)
{
    std::vector<std::string> out;
    if (requested.empty())
        return out;

    out.push_back(requested);
    if (!substitutes)
        return out;

    size_t const dot = extension_at(requested);
    if (dot == std::string::npos)
        return out;

    std::string const stem = requested.substr(0, dot);
    std::string const had = requested.substr(dot);

    for (char const *ext : kExtensions)
    {
        // Case-insensitively: the data has FOO.WAV as well as foo.wav, and
        // offering the same file twice would only slow the miss down.
        if (strcasecmp(had.c_str(), ext) == 0)
            continue;
        out.push_back(stem + ext);
    }
    return out;
}

}
