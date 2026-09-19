#include <doctest/doctest.h>

#include <initializer_list>
#include <string.h>

#include "i18n/language.h"

using namespace abuse::i18n;

TEST_CASE("language names parse and round trip") {
    Language l = Language::English;
    for (char const *name : { "en", "fr", "de", "pt_BR", "xx_XX" })
    {
        REQUIRE(parse_language(name, l));
        CHECK(strcasecmp(language_name(l), name) == 0);
    }
}

TEST_CASE("an unknown language is rejected and keeps the current one") {
    Language l = Language::French;
    CHECK_FALSE(parse_language("klingon", l));
    CHECK_FALSE(parse_language("", l));
    CHECK_FALSE(parse_language(nullptr, l));
    CHECK(l == Language::French);
}

// English is the base table, always loaded; the others are overlaid on it, so
// English itself has no file to overlay.
TEST_CASE("only the non-English languages name a file") {
    CHECK(language_lisp_file(Language::English) == nullptr);
    CHECK(strcmp(language_lisp_file(Language::French), "lisp/french.lsp") == 0);
    CHECK(strcmp(language_lisp_file(Language::German), "lisp/german.lsp") == 0);
    CHECK(strcmp(language_lisp_file(Language::Portuguese), "lisp/portuguese.lsp") == 0);

    // The pseudo language is generated, not loaded from anywhere.
    CHECK(language_lisp_file(Language::Pseudo) == nullptr);
}

TEST_CASE("the pseudo translation lengthens the text") {
    char const *out = pseudo_translate("Quit");
    REQUIRE(out != nullptr);
    CHECK(strlen(out) >= strlen("Quit") * 14 / 10);
}

// CP437, the encoding the shipped translations and the game font use. One
// byte per glyph, so the padded length is what the widget has to fit.
TEST_CASE("the pseudo translation accents the vowels") {
    char const *out = pseudo_translate("save");
    CHECK(strchr(out, '\xa0') != nullptr);   // a acute
    CHECK(strchr(out, '\x82') != nullptr);   // e acute
    CHECK(strchr(out, 'a') == nullptr);
    CHECK(strchr(out, 'e') == nullptr);
}

// Mangling a format specifier would corrupt the printf that consumes it.
TEST_CASE("format specifiers survive the pseudo translation") {
    char const *out = pseudo_translate("%s : %c%c");
    CHECK(strstr(out, "%s") != nullptr);
    CHECK(strstr(out, "%c") != nullptr);
}

TEST_CASE("the pseudo translation marks its padding") {
    char const *out = pseudo_translate("Load");
    // The padding is bracketed so a truncated string is recognisable as one.
    CHECK(strchr(out, '[') != nullptr);
    CHECK(strchr(out, ']') != nullptr);
}

// symbol_str hands out the result directly, and a caller may format two
// strings into one message, so one shared buffer would corrupt the first.
TEST_CASE("several pseudo translations can be held at once") {
    char const *a = pseudo_translate("first");
    char const *b = pseudo_translate("second");
    CHECK(a != b);
    CHECK(strstr(a, "f") != nullptr);
    CHECK(strstr(b, "s") != nullptr);
}

TEST_CASE("the pseudo translation tolerates empty and null input") {
    CHECK(pseudo_translate(nullptr) == nullptr);
    char const *out = pseudo_translate("");
    REQUIRE(out != nullptr);
}

TEST_CASE("a very long string does not overflow the buffer") {
    char big[2048];
    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = 0;

    char const *out = pseudo_translate(big);
    REQUIRE(out != nullptr);
    CHECK(strlen(out) < 1024);
}
