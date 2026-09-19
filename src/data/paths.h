/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Data path resolution per mode (Original / Remaster), XDG aware.
 *  Phase 1, task 1.3.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_DATA_PATHS_H_
#define ABUSE_DATA_PATHS_H_

#include <string>

namespace abuse::data {

enum class Mode
{
    Original,   // untouched 1995/1996 data, user-supplied
    Remaster    // free data shipped with the game
};

// Set once at startup, before setup(), by argument parsing in the harness.
// Isolated state: the engine reads the data prefix from imlib (specs.cpp),
// so these are the only mode-aware entry points. See AGENTS.md section 6.
void set_mode(Mode m);
Mode mode();

// --classic-data <dir>: explicit location of the original data. Wins over
// the default when the mode is Original.
void set_classic_data(std::string dir);
bool has_classic_data();

// Whether the classic sound is actually on disk, which is a different question
// from whether a directory was named: --classic-data can point at a directory
// that does not exist yet. Looks for one known effect, because the whole point
// of the Original mode is the sound.
bool classic_data_present();

// The page the data comes from, for the screen that explains how to get it.
char const *classic_data_url();
std::string classic_data_dir();

// Environment snapshot, so the resolver is pure and unit-testable.
struct Env
{
    std::string home;
    std::string xdg_data_home;
    std::string xdg_config_home;
};
Env system_env();

// XDG base directories with their defaults.
std::string data_home(const Env &env);    // $XDG_DATA_HOME or ~/.local/share
std::string config_home(const Env &env);  // $XDG_CONFIG_HOME or ~/.config

// Per-mode directories. Both end with a slash, like the engine prefixes.
std::string config_dir(Mode m, const Env &env);    // <config>/abuse/<mode>/
std::string writable_dir(Mode m, const Env &env);   // <data>/abuse/<mode>/

// Default home of the original data, filled by the phase 1 downloader.
std::string classic_data_default(const Env &env);

// Legacy ~/.abuse/ from Abuse-SDL, still honored when it exists so current
// installs keep their saves, light.tbl and abuserc.
bool legacy_dir_exists(const Env &env);
std::string legacy_dir(const Env &env);

// What setup() should use for the save prefix and the abuserc location.
// The legacy directory wins while it exists; a fresh install gets XDG.
std::string save_prefix(const Env &env);
std::string rc_path(const Env &env);

} // namespace abuse::data

#endif
