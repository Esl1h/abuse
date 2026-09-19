/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "compat.h"
#include <vector>

#include "harness.h"

#include "data/paths.h"

#include "demo.h"
#include "game.h"
#include "level.h"
#include "objects.h"
#include "chars.h"
#include "jrand.h"
#include "video.h"
#include "ui/options_screen.h"
#include "ui/classic_data_screen.h"
#include "ui/language_screen.h"
#include "configuration.h"
#include "lisp.h"
#include "lisp_gc.h"

namespace abuse::harness {

namespace {

struct Options {
    bool headless = false;
    bool want_hash = false;
    bool dump_bindings = false;
    bool dump_window = false;
    bool mode_given = false;
    enum class Screen { None, Options, Rebind, ClassicData, MenuHint, Language };
    Screen dump_screen = Screen::None;
    int window_w = 0;
    int window_h = 0;
    long hash_every = 0;        // 0 = only when the run ends
    long seed = -1;             // < 0 = leave jrand_init's value alone
    long max_ticks = -1;        // < 0 = no limit
    char *record = nullptr;
    char *playback = nullptr;
    char *level = nullptr;
    std::vector<long> dump_ticks;
    char const *out_dir = ".";
};

Options opt;
bool demo_running = false;

// Counted here rather than read from the level: at the title screen there is
// no level, and its tick counter would sit at zero forever.
uint64_t loop_ticks = 0;

// Playback ends by tearing the level down, so the hash has to be captured
// while it still exists rather than recomputed at the end of the run.
bool have_last = false;
uint64_t last_hash = 0;
uint64_t last_loop = 0;
uint32_t last_level_tick = 0;

// Consumes the value that follows a switch, or dies: a scripted run that
// silently ignores a malformed argument is worse than one that stops.
char *take_value(int argc, char **argv, int &i, char const *what)
{
    if (i + 1 >= argc)
    {
        fprintf(stderr, "%s requires an argument\n", what);
        exit(2);
    }
    return argv[++i];
}

long take_number(int argc, char **argv, int &i, char const *what)
{
    char *end;
    char *raw = take_value(argc, argv, i, what);
    long value = strtol(raw, &end, 10);
    if (*end || end == raw)
    {
        fprintf(stderr, "%s expects a number, got '%s'\n", what, raw);
        exit(2);
    }
    return value;
}

// The engine resolves relative names against the data directory prefix, which
// is right for game assets and wrong for a path the user typed. Make replay
// paths absolute so they mean what the shell means.
char *absolute(char *path)
{
    if (!path || path[0] == '/')
        return path;

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd)))
        return path;

    size_t len = strlen(cwd) + 1 + strlen(path) + 1;
    char *full = (char *)malloc(len);
    if (!full)
        return path;
    snprintf(full, len, "%s/%s", cwd, path);
    return full;
}

void parse_tick_list(char const *raw)
{
    char const *p = raw;
    while (*p)
    {
        char *end;
        long tick = strtol(p, &end, 10);
        if (end == p)
        {
            fprintf(stderr, "--dump-frames expects a comma separated list, got '%s'\n", raw);
            exit(2);
        }
        opt.dump_ticks.push_back(tick);
        p = end;
        while (*p == ',')
            p++;
    }
}

// FNV-1a. The state is integer only, so no float formatting ambiguity.
inline void mix(uint64_t &h, uint64_t value)
{
    for (int i = 0; i < 8; i++)
    {
        h ^= (value >> (i * 8)) & 0xff;
        h *= 0x100000001b3ULL;
    }
}

}

void parse_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--headless"))
            opt.headless = true;
        else if (!strcmp(argv[i], "--state-hash"))
            opt.want_hash = true;
        else if (!strcmp(argv[i], "--dump-bindings"))
            opt.dump_bindings = true;
        else if (!strcmp(argv[i], "--dump-window"))
            opt.dump_window = true;
        else if (!strcmp(argv[i], "--dump-options"))
            opt.dump_screen = Options::Screen::Options;
        else if (!strcmp(argv[i], "--dump-controls"))
            opt.dump_screen = Options::Screen::Rebind;
        else if (!strcmp(argv[i], "--dump-classic-data"))
            opt.dump_screen = Options::Screen::ClassicData;
        else if (!strcmp(argv[i], "--dump-menu-hint"))
            opt.dump_screen = Options::Screen::MenuHint;
        else if (!strcmp(argv[i], "--dump-language"))
            opt.dump_screen = Options::Screen::Language;
        else if (!strcmp(argv[i], "--window-size"))
        {
            opt.window_w = (int)take_number(argc, argv, i, "--window-size");
            opt.window_h = (int)take_number(argc, argv, i, "--window-size");
        }
        else if (!strcmp(argv[i], "--hash-every"))
            opt.hash_every = take_number(argc, argv, i, "--hash-every");
        else if (!strcmp(argv[i], "--seed"))
            opt.seed = take_number(argc, argv, i, "--seed");
        else if (!strcmp(argv[i], "--max-ticks"))
            opt.max_ticks = take_number(argc, argv, i, "--max-ticks");
        else if (!strcmp(argv[i], "--record"))
            opt.record = take_value(argc, argv, i, "--record");
        else if (!strcmp(argv[i], "--playback"))
            opt.playback = take_value(argc, argv, i, "--playback");
        else if (!strcmp(argv[i], "--level"))
            opt.level = take_value(argc, argv, i, "--level");
        else if (!strcmp(argv[i], "--dump-frames"))
            parse_tick_list(take_value(argc, argv, i, "--dump-frames"));
        else if (!strcmp(argv[i], "--out"))
            opt.out_dir = take_value(argc, argv, i, "--out");
        else if (!strcmp(argv[i], "--mode"))
        {
            char *value = take_value(argc, argv, i, "--mode");
            abuse::data::Mode m;
            if (abuse::data::parse_mode(value, m))
            {
                abuse::data::set_mode(m);
                opt.mode_given = true;
            }
            else
            {
                fprintf(stderr, "--mode expects original or remaster, got '%s'\n",
                        value);
                exit(2);
            }
        }
        else if (!strcmp(argv[i], "--classic-data"))
            abuse::data::set_classic_data(
                take_value(argc, argv, i, "--classic-data"));
    }

    if (opt.record && opt.playback)
    {
        fprintf(stderr, "--record and --playback are mutually exclusive\n");
        exit(2);
    }

    opt.record = absolute(opt.record);
    opt.playback = absolute(opt.playback);

    if (opt.record && !opt.level)
    {
        // start_recording() needs a level; without one it would fail at a
        // point where the only recourse is to read the source.
        fprintf(stderr, "--record also needs --level <file>\n");
        exit(2);
    }

    if (opt.headless && !opt.level && !opt.playback)
    {
        // main_menu() is a modal loop of its own, so tick() never runs and
        // --max-ticks cannot fire. Say so rather than hang silently.
        fprintf(stderr, "warning: --headless without --level or --playback "
                        "stops at the main menu and will not exit\n");
    }

    if (opt.headless)
    {
        // Set before SDL_Init, which setup() calls.
        abuse::set_env("SDL_VIDEO_DRIVER", "dummy");
        abuse::set_env("SDL_VIDEODRIVER", "dummy");
        abuse::set_env("SDL_AUDIO_DRIVER", "dummy");
        abuse::set_env("SDL_AUDIODRIVER", "dummy");
    }
}

bool window_size(int &w, int &h)
{
    if (opt.window_w < 1 || opt.window_h < 1)
        return false;
    w = opt.window_w;
    h = opt.window_h;
    return true;
}

bool mode_from_command_line()
{
    return opt.mode_given;
}

bool headless()
{
    return opt.headless;
}

void apply_seed()
{
    if (opt.seed >= 0)
        rand_on = (unsigned short)(opt.seed & (RAND_TABLE_SIZE - 1));
}

void before_game()
{
    if (!opt.headless)
        return;

    // gamma_correct() shows a modal calibration screen unless darkest_gray is
    // already defined. Define it so a scripted run does not stop there. The
    // value only affects the palette ramp, never game state. gamma_correct()
    // pins the value it actually uses, because startup.lsp loads gamma.lsp
    // after this point.
    LSymbol *gs = LSymbol::Find("darkest_gray");
    if (!gs || !DEFINEDP(gs->GetValue()))
    {
        LSpace *sp = LSpace::Current;
        LSpace::Current = &LSpace::Perm;
        LSymbol::FindOrCreate("darkest_gray")->SetNumber(16);
        LSpace::Current = sp;
    }
}

bool start_demo()
{
    if (opt.dump_bindings)
        print_action_map();

    if (opt.level)
    {
        the_game->load_level(opt.level);
        if (!current_level)
        {
            fprintf(stderr, "unable to load level '%s'\n", opt.level);
            return false;
        }
        the_game->set_state(RUN_STATE);
    }

    if (opt.playback)
    {
        if (!demo_man.set_state(demo_manager::PLAYING, opt.playback))
        {
            fprintf(stderr, "unable to play back '%s'\n", opt.playback);
            return false;
        }
        demo_running = true;
    }
    else if (opt.record)
    {
        if (!demo_man.set_state(demo_manager::RECORDING, opt.record))
        {
            fprintf(stderr, "unable to record to '%s'\n", opt.record);
            return false;
        }
        demo_running = true;
    }
    return true;
}

uint64_t state_hash()
{
    uint64_t h = 0xcbf29ce484222325ULL;

    if (!current_level)
    {
        mix(h, 0);
        return h;
    }

    mix(h, current_level->tick_counter());
    mix(h, rand_on);

    uint64_t count = 0;
    // first_object() walks the level list in load order, which does not depend
    // on what happens to be on screen. See ARCHITECTURE.md section 4.
    for (game_object *o = current_level->first_object(); o; o = o->next)
    {
        count++;
        mix(h, (uint64_t)(uint32_t)o->x);
        mix(h, (uint64_t)(uint32_t)o->y);
        mix(h, (uint64_t)(uint32_t)o->Xvel);
        mix(h, (uint64_t)(uint32_t)o->Yvel);
        mix(h, o->otype);
        mix(h, (uint64_t)o->state);
        mix(h, o->Hp);
        mix(h, o->Mp);
        mix(h, o->Aitype);
        mix(h, o->Aistate);
        mix(h, o->Aistate_time);
        mix(h, (uint64_t)(uint8_t)o->direction);
        mix(h, (uint64_t)(uint16_t)o->current_frame);

        // Local variables carry ammo, timers and everything the Lisp side
        // keeps per object.
        if (o->lvars)
        {
            int total = figures[o->otype]->tv;
            for (int i = 0; i < total; i++)
                mix(h, (uint64_t)(uint32_t)o->lvars[i]);
        }
    }
    mix(h, count);

    return h;
}

namespace {

void report(char const *why, uint64_t loop, uint32_t level_tick, uint64_t hash)
{
    printf("%s loop=%llu level_tick=%u hash=%016llx\n", why,
           (unsigned long long)loop, level_tick, (unsigned long long)hash);
    fflush(stdout);
}

}

bool tick()
{
    if (opt.want_hash && current_level)
    {
        last_hash = state_hash();
        last_loop = loop_ticks;
        last_level_tick = current_level->tick_counter();
        have_last = true;

        if (opt.hash_every > 0 && loop_ticks % (uint64_t)opt.hash_every == 0)
            report("tick", last_loop, last_level_tick, last_hash);
    }

    // A playback that runs out of packets drops back to NORMAL on its own.
    if (demo_running && opt.playback && demo_man.current_state() == demo_manager::NORMAL)
        return false;

    if (opt.max_ticks >= 0 && loop_ticks >= (uint64_t)opt.max_ticks)
        return false;

    loop_ticks++;
    return true;
}

namespace {

bool tick_is_wanted()
{
    for (long t : opt.dump_ticks)
        if ((uint64_t)t == loop_ticks)
            return true;
    return false;
}

}

void before_frame()
{
    if (opt.dump_ticks.empty() || !tick_is_wanted())
        return;

    if (!opt.dump_window)
        return;

    char path[512];
    snprintf(path, sizeof(path), "%s/%06llu-window.bmp", opt.out_dir,
             (unsigned long long)loop_ticks);
    request_window_capture(path);
}

bool draw_overlay_for_capture()
{
    if (opt.dump_screen == Options::Screen::None || opt.dump_ticks.empty()
        || !tick_is_wanted())
        return false;

    // The only way a scripted run can see the options screen, which otherwise
    // exists only inside its own event loop.
    if (opt.dump_screen == Options::Screen::Rebind)
        abuse::ui::draw_rebind_screen(0, false);
    else if (opt.dump_screen == Options::Screen::ClassicData)
        abuse::ui::draw_classic_data_screen(0);
    else if (opt.dump_screen == Options::Screen::MenuHint)
        abuse::ui::draw_options_hint();
    else if (opt.dump_screen == Options::Screen::Language)
    {
        // The whole screen, art included, which is what the player sees.
        abuse::ui::draw_language_screen(0, abuse::ui::draw_language_backdrop());
    }
    else
        abuse::ui::draw_options_screen(0);
    return true;
}

void after_frame()
{
    if (opt.dump_ticks.empty() || !tick_is_wanted())
        return;

    char path[512];
    snprintf(path, sizeof(path), "%s/%06llu.bmp", opt.out_dir,
             (unsigned long long)loop_ticks);
    if (!save_frame_bmp(path))
        fprintf(stderr, "unable to write '%s'\n", path);
}

void finish()
{
    if (opt.want_hash)
    {
        if (have_last)
            report("final", last_loop, last_level_tick, last_hash);
        else
            report("final", loop_ticks, 0, state_hash());
    }

    if (demo_running && demo_man.current_state() == demo_manager::RECORDING)
        demo_man.set_state(demo_manager::NORMAL);
}

}
