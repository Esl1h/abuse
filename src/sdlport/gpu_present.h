/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Presenting the frame through SDL_GPU. Phase 6, block 6.3.
 *
 *  The game has drawn into an 8-bit indexed buffer since 1995, and turning
 *  that into colour has been a CPU loop ever since: one lookup per pixel
 *  per frame, two million of them a frame at 1080p. That is the ceiling
 *  the visual phase keeps running into, and it is why the decision of
 *  2026-09-20 was SDL_GPU.
 *
 *  Here the indexed buffer goes up as an R8 texture and the palette as a
 *  256x1 one, and a fragment shader does the lookup. The overlay goes up
 *  as ARGB and is composited in a second draw.
 *
 *  **Opt-in, and not the default.** The SDL_Renderer path is what every
 *  snapshot in the suite was taken through, and it stays the reference
 *  until this one can reproduce them. Only Vulkan so far, because only
 *  SPIR-V is committed; on anything else start() fails and the caller
 *  keeps the old path.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_SDLPORT_GPU_PRESENT_H_
#define ABUSE_SDLPORT_GPU_PRESENT_H_

#include <stdint.h>

struct SDL_Window;

namespace abuse::sdlport::gpu {

// Builds the device, the pipelines and the samplers. False when SDL_GPU
// cannot be had, which is not an error: the caller presents the old way.
bool start(SDL_Window *window);

void stop();
bool running();

// The 256 entries as the game's palette holds them, 0-255 per channel.
void set_palette(uint8_t const rgb[768]);

// One frame. `indexed` is the game buffer, `overlay` is the native
// resolution layer or null. The game picture is letterboxed inside the
// window according to the scale mode; the overlay covers the window.
void present(uint8_t const *indexed, int w, int h, int pitch,
             uint32_t const *overlay, int ow, int oh, int opitch);

// The window in pixels, and where the game picture sits inside it.
//
// The native resolution overlay needs both: it covers the window, and the
// HUD inside it is measured against the picture rather than the window, so
// that it keeps its proportion when there are letterbox bars. They come
// from here rather than being worked out again on the other side, because
// the two answers have to be the same one: the presenter positions the
// picture with exactly this calculation.
//
// False when this path is not running, and the caller asks SDL_Renderer.
bool window_size(int &w, int &h);
bool picture_rect(int game_w, int game_h, int &x, int &y, int &w, int &h);

// Saves the next frame to `path` as BMP. The same thing --dump-window does
// on the old path, and the only way to see what this one produces without
// a person at the screen.
//
// It costs a render target and a round trip, so it happens only when asked
// and never during ordinary play.
void capture_next_frame(char const *path);

}

#endif
