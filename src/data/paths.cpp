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

#include <string.h>

#include <cstdlib>

namespace abuse::data {

namespace {

Mode g_mode = Mode::Remaster;
std::string g_classic_data;
bool g_has_classic_data = false;

// The directory a mode keeps its saves and config in. Deliberately "classic"
// and not "original": that is the name on disk since phase 1, and changing it
// would move everyone's saves.
std::string mode_dir_name(Mode m)
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

char const *mode_name(Mode m)
{
    return m == Mode::Original ? "original" : "remaster";
}

bool parse_mode(char const *name, Mode &out)
{
    if (!name)
        return false;
    if (strcasecmp(name, "original") == 0)
    {
        out = Mode::Original;
        return true;
    }
    if (strcasecmp(name, "remaster") == 0)
    {
        out = Mode::Remaster;
        return true;
    }
    return false;
}

std::string mode_file(const Env &env)
{
    // Wherever the rest of the configuration is. An install that still has
    // the legacy directory keeps everything inside it, so that deleting that
    // one directory still uninstalls the game's state.
    if (legacy_dir_exists(env))
        return legacy_dir(env) + "mode";
    return config_home(env) + "/abuse/mode";
}

namespace {

// mkdir -p for the directory a file is going into. On a machine with the
// legacy ~/.abuse the XDG tree is never created, because nothing else the
// game writes goes there, so the first thing to write has to create it.
bool make_parent(std::string const &file)
{
    std::string::size_type slash = file.rfind('/');
    if (slash == std::string::npos)
        return true;

    std::string dir = file.substr(0, slash);
    for (std::string::size_type i = 1; i <= dir.size(); i++)
    {
        if (i < dir.size() && dir[i] != '/')
            continue;
        if (!abuse::make_directory(dir.substr(0, i).c_str()))
            return false;
    }
    return true;
}

}

bool load_saved_mode(const Env &env, Mode &out)
{
    FILE *f = fopen(mode_file(env).c_str(), "rb");
    if (!f)
        return false;

    char buf[32] = {};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    // Trim whatever an editor left behind.
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' '))
        buf[--n] = 0;

    return parse_mode(buf, out);
}

bool save_mode(const Env &env, Mode m)
{
    std::string path = mode_file(env);
    if (!make_parent(path))
        return false;

    FILE *f = fopen(path.c_str(), "wb");
    if (!f)
        return false;
    bool ok = fprintf(f, "%s\n", mode_name(m)) > 0;
    fclose(f);
    return ok;
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
    return config_home(env) + "/abuse/" + mode_dir_name(m) + "/";
}

std::string writable_dir(Mode m, const Env &env)
{
    return data_home(env) + "/abuse/" + mode_dir_name(m) + "/";
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
