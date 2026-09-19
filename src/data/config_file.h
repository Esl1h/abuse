/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Writing single keys back to abuserc. Phase 4, task 4.5.
 *
 *  The file is the player's, hand edited and commented, so a setting changed
 *  in the options screen replaces one line and leaves every other byte alone.
 *  The transformation is a pure function so it can be tested without a disk.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_DATA_CONFIG_FILE_H_
#define ABUSE_DATA_CONFIG_FILE_H_

#include <string>
#include <vector>

namespace abuse::data {

// Returns `text` with `key` set to `value`. An existing assignment is replaced
// in place, keeping its position in the file; a key that appears only as a
// commented out example is uncommented; anything else is appended. Comments,
// blank lines and unknown keys are preserved exactly.
std::string set_config_key(std::string const &text, std::string const &key,
                           std::string const &value);

// Replaces every assignment of `key` with one line per value, written where
// the first of them was. For keys that legitimately repeat, which `bind=` is:
// the rebind screen writes the whole set at once, because a binding removed
// has to disappear from the file rather than linger. Commented lines are left
// alone, unlike in set_config_key: for a repeating key a commented example is
// documentation of the syntax, not a disabled setting.
std::string set_config_lines(std::string const &text, std::string const &key,
                             std::vector<std::string> const &values);

bool save_config_lines(char const *path, char const *key,
                       std::vector<std::string> const &values);

// Reads `path`, applies set_config_key and writes it back. False when the file
// could not be written, which the caller should report rather than hide: a
// setting the player changed and that did not survive the session is worse
// than one that refused to change.
bool save_config_key(char const *path, char const *key, char const *value);

}

#endif
