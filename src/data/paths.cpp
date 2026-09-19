/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See paths.h.
 *
 *  This software was released into the Public Domain.
 */

#include "paths.h"

#include <stdio.h>

#include "compat.h"

#include <cstdlib>

namespace abuse::data {

namespace {

Mode g_mode = Mode::Remaster;
std::string g_classic_data;
bool g_has_classic_data = false;

std::string mode_name(Mode m)
{
    return m == Mode::Original ? "classic" : "remaster";
}

} // namespace

void set_mode(Mode m)
{
    g_mode = m;
}

Mode mode()
{
    return g_mode;
}

void set_classic_data(std::string dir)
{
    g_classic_data = std::move(dir);
    if (!g_classic_data.empty() && g_classic_data.back() != '/')
        g_classic_data += '/';
    g_has_classic_data = !g_classic_data.empty();
}

bool has_classic_data()
{
    return g_has_classic_data;
}

bool classic_data_present()
{
    std::string probe = classic_data_dir() + "sfx/ambcave1.wav";
    FILE *f = fopen(probe.c_str(), "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

char const *classic_data_url()
{
    // Sam Hocevar's page, which is where the tarballs the fetch script pulls
    // actually live. ABUSE_CLASSIC_URL overrides the script's mirror; this is
    // the page a person reads.
    return "http://abuse.zoy.org/wiki/download";
}

std::string classic_data_dir()
{
    if (g_has_classic_data)
        return g_classic_data;
    return classic_data_default(system_env());
}

Env system_env()
{
    Env env;
    if (const char *home = std::getenv("HOME"))
        env.home = home;
    if (const char *data = std::getenv("XDG_DATA_HOME"))
        env.xdg_data_home = data;
    if (const char *config = std::getenv("XDG_CONFIG_HOME"))
        env.xdg_config_home = config;
    return env;
}

std::string data_home(const Env &env)
{
    if (!env.xdg_data_home.empty())
        return env.xdg_data_home;
    return env.home + "/.local/share";
}

std::string config_home(const Env &env)
{
    if (!env.xdg_config_home.empty())
        return env.xdg_config_home;
    return env.home + "/.config";
}

std::string config_dir(Mode m, const Env &env)
{
    return config_home(env) + "/abuse/" + mode_name(m) + "/";
}

std::string writable_dir(Mode m, const Env &env)
{
    return data_home(env) + "/abuse/" + mode_name(m) + "/";
}

std::string classic_data_default(const Env &env)
{
    return data_home(env) + "/abuse/classic/";
}

bool legacy_dir_exists(const Env &env)
{
    if (env.home.empty())
        return false;
    struct stat st;
    return stat((env.home + "/.abuse").c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string legacy_dir(const Env &env)
{
    return env.home + "/.abuse/";
}

std::string save_prefix(const Env &env)
{
    if (legacy_dir_exists(env))
        return legacy_dir(env);
    return writable_dir(mode(), env);
}

std::string rc_path(const Env &env)
{
    if (legacy_dir_exists(env))
        return legacy_dir(env) + "abuserc";
    return config_dir(mode(), env) + "abuserc";
}

} // namespace abuse::data
