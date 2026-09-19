/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Language selection. Phase 4, task 4.3.
 *
 *  The strings the player reads live in data/lisp/<language>.lsp as symbol
 *  definitions, and only C++ reads them, through symbol_str(). No other lisp
 *  code references them, which is what makes it safe to swap the table.
 *
 *  English is always loaded first and the chosen language is loaded on top of
 *  it, so a symbol the translation has not covered falls back to English
 *  instead of showing "Missing language symbol!".
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_I18N_LANGUAGE_H_
#define ABUSE_I18N_LANGUAGE_H_

namespace abuse::i18n {

enum class Language
{
    English,
    French,
    German,
    Portuguese,
    // Not a language: English run through a transform that makes it longer and
    // accented, so layout problems show up without waiting for a translation.
    Pseudo
};

// The single-byte encoding a language's strings are written in. The engine
// indexes glyphs by byte and measures text in bytes, so each language is one
// codepage rather than UTF-8.
enum class Encoding
{
    // What the shipped french.lsp and german.lsp use, and what the art fonts
    // are laid out in.
    CP437,
    // Portuguese needs a-tilde and o-tilde, which CP437 does not have.
    Latin1
};

bool parse_language(char const *name, Language &out);
char const *language_name(Language l);
Encoding language_encoding(Language l);

// The file loaded on top of English, or NULL when English itself is chosen.
char const *language_lisp_file(Language l);

Language &language();

// Asks SDL what the user's system is set to and maps it to something we have.
// Falls back to English when there is no match.
Language detect_language();

// True while the pseudo language is selected, which is what tells symbol_str
// to transform what it returns.
bool pseudo_active();

// Lengthens and accents a string, in place of a translation. The result lives
// in a rotating set of internal buffers, so a caller may hold a few at once.
char const *pseudo_translate(char const *text);

}

#endif
