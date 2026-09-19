/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See uitext.h.
 *
 *  This software was released into the Public Domain.
 */

#include "uitext.h"

#include "language.h"

namespace abuse::i18n {

char const *say(Phrase const &p)
{
    // The pseudo language is generated from English, and these strings go
    // through it the same way symbol_str's do, so a phrase that does not fit
    // its panel shows up in the same pass as the rest of the text.
    if (language() == Language::Portuguese && p.pt)
        return p.pt;

    if (pseudo_active())
        return pseudo_translate(p.en);

    return p.en;
}

}
