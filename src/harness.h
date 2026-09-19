/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Test harness: command line switches that make a run reproducible and
 *  observable, so replays can be compared byte for byte in CI.
 *
 *  This software was released into the Public Domain.
 */

#ifndef __HARNESS_H_
#define __HARNESS_H_

#include <stdint.h>

namespace abuse::harness {

// Parsed once, before SDL is touched, so that --headless can pick the dummy
// drivers. Unknown arguments are left alone for the existing parsers.
void parse_args(int argc, char **argv);

bool headless();

// --frame-alpha F. Forces the interpolated draw at a fixed point between two
// ticks, which is the only way a scripted capture can show it: the harness
// runs one frame per tick, where there is nothing to blend.
bool frame_alpha(float &out);

// --level-info: prints the size of the level that was loaded, in tiles and
// in pixels, and how much of it a viewport of each aspect ratio would need.
// Read-only, and the run ends right after.
//
// This is the measurement phase 6.5 waits on: the levels were drawn for
// 4:3, and the question that decides whether widescreen is possible at all
// is how much room there is at the edges.
bool want_level_info();
void print_level_info(int fg_tiles_x, int fg_tiles_y, int tile_w, int tile_h,
                      int bg_tiles_x, int bg_tiles_y, int bg_tile_w,
                      int bg_tile_h, int bg_empty);

// --input-script <file>: a player driven from a text file instead of from a
// keyboard, one line per stretch of ticks:
//
//     # comment
//     30 right
//     10 right fire
//     15 none
//
// The three replays this project started with are 400 ticks of nobody
// touching anything, which is how an interpolation bug reached a human
// before it reached a test. This is how a replay with movement gets made:
// run with --level and --record, and the game writes a real recording of
// what the script did.
//
// Consumed one tick per call, which is how often the engine asks for input.
// False when no script is loaded, and the keyboard answers as usual.
bool scripted_input(uint8_t &flags);

// True when --mode was given, so the remembered mode knows to stand aside:
// a flag the player typed beats a file they forgot about.
bool mode_from_command_line();

// --window-size W H. What the window snapshots compare is the presented frame,
// and scale mode, filter and letterbox all depend on the shape of the window
// it was presented into, so a scripted run has to pin it. False when the run
// did not ask for one.
bool window_size(int &w, int &h);

// Applied right after jrand_init(), which would otherwise seed the random
// cursor from the wall clock. See ARCHITECTURE.md section 5.
void apply_seed();

// Called after Lisp::Init() and before the Game is built. In headless mode it
// answers the gamma calibration prompt, which would otherwise block forever
// waiting for a keypress that cannot arrive.
void before_game();

// Starts recording or playback once a Game exists. Returns false when the
// requested demo could not be started, which is a fatal condition for a
// scripted run.
bool start_demo();

// Called once per iteration of the main loop, after the world has stepped.
// Returns false when the run should end.
bool tick();

// Called right before the frame is drawn, so --dump-window can arm the capture
// that has to happen inside the present.
void before_frame();

// Called while the frame is being drawn, from the one place that decides what
// goes on the overlay. Returns true when --dump-options has put the options
// screen there, so the caller leaves it alone: the overlay is cleared every
// frame, and drawing it any earlier would only have it wiped.
bool draw_overlay_for_capture();

// Called right after the frame has been presented, so that --dump-frames
// captures what was actually drawn for this tick.
void after_frame();

// Called after the run, before teardown.
void finish();

// Hash of the integer game state: every object's position, velocity, health,
// AI state and local variables, plus the random cursor and the tick counter.
uint64_t state_hash();

}

#endif
