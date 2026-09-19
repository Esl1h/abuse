#include <doctest/doctest.h>

#include <initializer_list>
#include <string.h>

#include "keys.h"

TEST_CASE("key_value resolves the named keys") {
    CHECK(key_value("Backspace") == JK_BACKSPACE);
    CHECK(key_value("Tab") == JK_TAB);
    CHECK(key_value("Enter") == JK_ENTER);
    CHECK(key_value("ESC") == JK_ESC);
    CHECK(key_value("Space") == JK_SPACE);
    CHECK(key_value("Up") == JK_UP);
    CHECK(key_value("Down") == JK_DOWN);
    CHECK(key_value("Left") == JK_LEFT);
    CHECK(key_value("Right") == JK_RIGHT);
    CHECK(key_value("CTRL_L") == JK_CTRL_L);
    CHECK(key_value("CTRL_R") == JK_CTRL_R);
    CHECK(key_value("Insert") == JK_INSERT);
    CHECK(key_value("PageUp") == JK_PAGEUP);
    CHECK(key_value("PageDown") == JK_PAGEDOWN);
    CHECK(key_value("F1") == JK_F1);
    CHECK(key_value("F10") == JK_F10);
}

TEST_CASE("key_value ignores case") {
    CHECK(key_value("up") == JK_UP);
    CHECK(key_value("UP") == JK_UP);
    CHECK(key_value("uP") == JK_UP);
    CHECK(key_value("pageup") == JK_PAGEUP);
}

TEST_CASE("key_value falls back to the first character") {
    CHECK(key_value("a") == 'a');
    CHECK(key_value("w") == 'w');
    CHECK(key_value("1") == '1');
    // Only the first character is read, silently.
    CHECK(key_value("wasd") == 'w');
}

TEST_CASE("key_name spells out the special keys") {
    char buf[64];

    key_name(JK_UP, buf);
    CHECK(strcmp(buf, "Up Arrow") == 0);

    key_name(JK_CTRL_L, buf);
    CHECK(strcmp(buf, "Left Ctrl") == 0);

    key_name(JK_ESC, buf);
    CHECK(strcmp(buf, "Esc") == 0);

    key_name('a', buf);
    CHECK(strcmp(buf, "a") == 0);

    // Not printable and not special: empty, not left as garbage.
    key_name(1, buf);
    CHECK(buf[0] == '\0');
}

// key_name and key_value are not inverses, which matters for any rebinding UI
// that writes back what it displayed. Pinned here so a future change to either
// side is a deliberate one.
TEST_CASE("key_name and key_value do not round trip") {
    char buf[64];

    key_name(JK_UP, buf);
    CHECK(strcmp(buf, "Up Arrow") == 0);
    CHECK(key_value(buf) != JK_UP);
    CHECK(key_value(buf) == 'U');   // falls through to the first character

    key_name(JK_CTRL_L, buf);
    CHECK(key_value(buf) != JK_CTRL_L);

    // The keys that do survive a round trip are the ones abuserc uses.
    int const round_trips[] = { JK_BACKSPACE, JK_TAB, JK_ENTER, JK_ESC, JK_SPACE,
                                'a', 'z', '0' };
    for (int key : round_trips)
    {
        key_name(key, buf);
        CHECK(key_value(buf) == key);
    }
}
