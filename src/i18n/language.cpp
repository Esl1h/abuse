/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See language.h.
 *
 *  This software was released into the Public Domain.
 */

#include "language.h"

#include <string.h>
#include "compat.h"

#include <SDL3/SDL.h>

namespace abuse::i18n {

namespace {

Language g_language = Language::English;

struct Entry
{
    Language lang;
    char const *name;       // as written in abuserc
    char const *file;       // loaded on top of english.lsp, NULL for English
    char const *locale;     // prefix SDL reports, NULL when there is none
    Encoding encoding;      // how that file is written
};

Entry const kLanguages[] = {
    { Language::English,    "en",    NULL,                  "en", Encoding::CP437  },
    { Language::French,     "fr",    "lisp/french.lsp",     "fr", Encoding::CP437  },
    { Language::German,     "de",    "lisp/german.lsp",     "de", Encoding::CP437  },
    { Language::Portuguese, "pt_BR", "lisp/portuguese.lsp", "pt", Encoding::Latin1 },
    { Language::Pseudo,     "xx_XX", NULL,                  NULL, Encoding::CP437  },
};

Entry const *find(Language l)
{
    for (Entry const &e : kLanguages)
        if (e.lang == l)
            return &e;
    return &kLanguages[0];
}

}

bool parse_language(char const *name, Language &out)
{
    if (!name)
        return false;
    for (Entry const &e : kLanguages)
        if (strcasecmp(e.name, name) == 0)
        {
            out = e.lang;
            return true;
        }
    return false;
}

char const *language_name(Language l)
{
    return find(l)->name;
}

char const *language_lisp_file(Language l)
{
    return find(l)->file;
}

Encoding language_encoding(Language l)
{
    return find(l)->encoding;
}

Language &language()
{
    return g_language;
}

Language detect_language()
{
    int count = 0;
    SDL_Locale **locales = SDL_GetPreferredLocales(&count);
    if (!locales)
        return Language::English;

    Language found = Language::English;
    // In order of the user's own preference, so a list of pt then en picks pt.
    for (int i = 0; i < count && locales[i]; i++)
    {
        char const *lang = locales[i]->language;
        if (!lang)
            continue;
        bool matched = false;
        for (Entry const &e : kLanguages)
        {
            if (e.locale && strcasecmp(e.locale, lang) == 0)
            {
                found = e.lang;
                matched = true;
                break;
            }
        }
        if (matched)
            break;
    }

    SDL_free(locales);
    return found;
}

bool pseudo_active()
{
    return g_language == Language::Pseudo;
}

char const *pseudo_translate(char const *text)
{
    // A handful of buffers, because a caller may format two strings into one
    // message before using either.
    static char buffers[4][1024];
    static int next = 0;

    if (!text)
        return text;

    char *out = buffers[next];
    next = (next + 1) % 4;

    // Accent the vowels and pad to about 140% of the original, which is
    // roughly how much longer German and Portuguese run than English. The
    // accents are CP437 bytes, the encoding french.lsp and german.lsp are
    // written in and the one the game font is laid out in; one byte is also
    // one glyph there, so the padded length is the length on screen.
    size_t n = 0;
    size_t const limit = sizeof(buffers[0]) - 8;
    for (char const *c = text; *c && n < limit; c++)
    {
        // Leave format specifiers alone: mangling "%s" would break printf.
        if (*c == '%')
        {
            out[n++] = *c;
            if (c[1] && n < limit)
                out[n++] = *++c;
            continue;
        }

        char accented = 0;
        switch (*c)
        {
        case 'a': accented = '\xa0'; break;   // a acute
        case 'e': accented = '\x82'; break;   // e acute
        case 'i': accented = '\xa1'; break;   // i acute
        case 'o': accented = '\xa2'; break;   // o acute
        case 'u': accented = '\xa3'; break;   // u acute
        case 'c': accented = '\x87'; break;   // c cedilla
        default: break;
        }

        out[n++] = accented ? accented : *c;
    }

    // The padding goes in brackets so it is obvious that it is not content,
    // and so a string cut short on screen is recognisable as cut short.
    size_t original = strlen(text);
    size_t want = original + original * 2 / 5;
    if (n + 2 < limit && want > original)
    {
        out[n++] = ' ';
        out[n++] = '[';
        for (size_t i = 0; i < want - original && n + 1 < limit; i++)
            out[n++] = '-';
        if (n + 1 < limit)
            out[n++] = ']';
    }

    out[n] = 0;
    return out;
}

}
