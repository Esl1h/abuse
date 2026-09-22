/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef _VIDEO_HPP_
#define _VIDEO_HPP_

#define TRI_1024x768x256 0x62
#define TRI_800x600x256  0x5e
#define TRI_640x480x256  0x5c
#define VGA_320x200x256  0x13
#define CGA_640x200x2    6
#define XWINDOWS_256     256
#define XWINDOWS_2       2

#include "image.h"

extern int xres,yres;
extern int xoff,yoff;
extern image *main_screen;

void set_mode(int argc=0, char **argv=NULL);
void close_graphics();
void update_window_done();

// Writes the current 8-bit frame, palette included, as a BMP.
// Returns false if the file could not be written.
bool save_frame_bmp(char const *path);

// Asks for the next presented frame to be written to `path`, at window size
// and after scaling, filtering and letterboxing. The 8-bit dump above is taken
// from the buffer the game draws into, before any of that happens, so it
// cannot see a wrong scale mode, a wrong filter or a misplaced letterbox.
// Reading has to happen before the present, which is why this is a request and
// not a call.
void request_window_capture(char const *path);

// The size in real pixels the overlay has to be, which is the window's and not
// the game's 320x200. False before the video is up.
bool window_pixel_size(int &w, int &h);

// Where a rectangle of the game's own 320x200 buffer lands in window pixels,
// after the logical presentation has scaled and letterboxed it. The overlay is
// in window pixels and the game is not, so anything that has to line up with
// what the game drew goes through here. False before the video is up.
bool game_rect_to_window(int gx, int gy, int gw, int gh,
                         int &x, int &y, int &w, int &h);

// Between the window's pixels and the game's, both ways.
//
// The game space here is the *logical* one, which for a 200 tall buffer is
// 240 tall: that is the aspect correction, and mouse_yscale is what the
// caller uses to get back to 200. Everything that reads or moves the
// pointer goes through these, because the two presentation paths letterbox
// the picture differently and only they know how.
void window_to_game(float wx, float wy, float &gx, float &gy);
void game_to_window(float gx, float gy, float &wx, float &wy);

// Re-apply the presentation and filter options to the live renderer.
void apply_presentation();
void apply_filter();

void update_dirty(image *im, int xoff=0, int yoff=0);
void put_part_image(image *im, int x, int y, int x1, int y1, int x2, int y2);
void put_image(image * im, int x, int y);

void clear_put_image(image *im, int x, int y);
int get_vmode();

#endif
