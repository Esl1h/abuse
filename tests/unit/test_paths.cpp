/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Unit tests for the per-mode path resolver (phase 1, task 1.3).
 *
 *  This software was released into the Public Domain.
 */

#include <doctest/doctest.h>

#include <stdlib.h>
#include <string.h>

#include <string>

#include "compat.h"
#include "data/paths.h"

using abuse::data::Env;
using abuse::data::Mode;

namespace {

// mkdir -p and rm -rf, enough for a test that needs a directory tree that is
// not there yet.
bool make_tree(std::string const &path)
{
    for (std::string::size_type i = 1; i <= path.size(); i++)
        if (i == path.size() || path[i] == '/')
            if (!abuse::make_directory(path.substr(0, i).c_str()))
                return false;
    return true;
}

void remove_tree(std::string const &path)
{
    std::string command = "rm -rf '" + path + "'";
    if (system(command.c_str()) != 0)
        FAIL("could not clear " << path);
}

Env make_env(const char *home, const char *xdg_data, const char *xdg_config)
{
    Env env;
    if (home)
        env.home = home;
    if (xdg_data)
        env.xdg_data_home = xdg_data;
    if (xdg_config)
        env.xdg_config_home = xdg_config;
    return env;
}

} // namespace

TEST_CASE("XDG defaults when the variables are empty")
{
    Env env = make_env("/home/player", "", "");

    CHECK(abuse::data::data_home(env) == "/home/player/.local/share");
    CHECK(abuse::data::config_home(env) == "/home/player/.config");
    CHECK(abuse::data::config_dir(Mode::Original, env)
          == "/home/player/.config/abuse/classic/");
    CHECK(abuse::data::config_dir(Mode::Remaster, env)
          == "/home/player/.config/abuse/remaster/");
    CHECK(abuse::data::writable_dir(Mode::Original, env)
          == "/home/player/.local/share/abuse/classic/");
    CHECK(abuse::data::writable_dir(Mode::Remaster, env)
          == "/home/player/.local/share/abuse/remaster/");
    CHECK(abuse::data::classic_data_default(env)
          == "/home/player/.local/share/abuse/classic/");
}

TEST_CASE("XDG variables set, Flatpak style")
{
    // Inside a Flatpak sandbox the variables point somewhere else entirely.
    Env env = make_env("/home/player", "/app/share", "/app/config");

    CHECK(abuse::data::data_home(env) == "/app/share");
    CHECK(abuse::data::config_home(env) == "/app/config");
    CHECK(abuse::data::config_dir(Mode::Remaster, env)
          == "/app/config/abuse/remaster/");
    CHECK(abuse::data::writable_dir(Mode::Original, env)
          == "/app/share/abuse/classic/");
}

TEST_CASE("legacy ~/.abuse wins while it exists")
{
    // Only the resolver's choice is tested here; the existence check itself
    // is against the real home, which belongs to the integration run.
    Env env = make_env("/home/player", "", "");
    CHECK(abuse::data::legacy_dir(env) == "/home/player/.abuse/");

    abuse::data::set_mode(Mode::Remaster);
    CHECK(abuse::data::rc_path(env) != "");
}

TEST_CASE("classic data override and default")
{
    abuse::data::set_mode(Mode::Original);

    abuse::data::set_classic_data("/opt/original");
    CHECK(abuse::data::has_classic_data());
    CHECK(abuse::data::classic_data_dir() == "/opt/original/");

    // Without the flag the XDG default applies.
    abuse::data::set_classic_data("");
    CHECK(!abuse::data::has_classic_data());
    CHECK(abuse::data::classic_data_dir()
          == abuse::data::classic_data_default(abuse::data::system_env()));
}

// Naming a directory is not the same as the data being in it: --classic-data
// can point somewhere that does not exist yet, and the Original mode is about
// the sound, so the sound is what gets looked for.
TEST_CASE("the classic data probe looks for a file, not a setting") {
    abuse::data::set_classic_data(ABUSE_TEST_BUILD_DIR "/no-such-classic");
    CHECK(abuse::data::has_classic_data());
    CHECK_FALSE(abuse::data::classic_data_present());
}

TEST_CASE("the download page is a real address") {
    char const *url = abuse::data::classic_data_url();
    REQUIRE(url != nullptr);
    CHECK(strncmp(url, "http", 4) == 0);
}

// The bug this covers: with a legacy ~/.abuse present the XDG tree is never
// created, because nothing else the game writes goes there, and the first
// write to <config>/abuse/mode failed with nowhere to put it.
TEST_CASE("the mode file follows the rest of the configuration") {
    std::string root = ABUSE_TEST_BUILD_DIR "/paths-mode";
    std::string home = root + "/home";
    std::string config = root + "/config";

    remove_tree(root);
    REQUIRE(make_tree(home));
    REQUIRE(make_tree(config));

    Env env = make_env(home.c_str(), "", config.c_str());

    SUBCASE("a fresh install writes under the config directory") {
        CHECK(abuse::data::mode_file(env) == config + "/abuse/mode");

        abuse::data::Mode got = Mode::Remaster;
        CHECK_FALSE(abuse::data::load_saved_mode(env, got));

        // The directory does not exist yet, which is the whole point.
        REQUIRE(abuse::data::save_mode(env, Mode::Original));
        REQUIRE(abuse::data::load_saved_mode(env, got));
        CHECK(got == Mode::Original);

        REQUIRE(abuse::data::save_mode(env, Mode::Remaster));
        REQUIRE(abuse::data::load_saved_mode(env, got));
        CHECK(got == Mode::Remaster);
    }

    SUBCASE("a legacy install keeps it inside ~/.abuse") {
        REQUIRE(make_tree(home + "/.abuse"));
        CHECK(abuse::data::mode_file(env) == home + "/.abuse/mode");

        REQUIRE(abuse::data::save_mode(env, Mode::Original));

        abuse::data::Mode got = Mode::Remaster;
        REQUIRE(abuse::data::load_saved_mode(env, got));
        CHECK(got == Mode::Original);

        // And nothing was left in the XDG tree.
        abuse::data::Mode ignored;
        Env xdg_only = make_env((root + "/nowhere").c_str(), "", config.c_str());
        CHECK_FALSE(abuse::data::load_saved_mode(xdg_only, ignored));
    }

    remove_tree(root);
}
