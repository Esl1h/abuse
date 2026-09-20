/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include <SDL3/SDL.h>

#include "common.h"

#include "filter.h"
#include "video.h"
#include <vector>

#include "render/options.h"
#include "render/lightmap.h"
#include "ui/overlay.h"
#include "harness.h"
#include "image.h"
#include "setup.h"
#include "errorui.h"

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Surface *surface = NULL;
SDL_Texture *texture = NULL;
image *main_screen = NULL;
float mouse_yscale;
int xres, yres;

extern palette *lastl;
extern flags_struct flags;

namespace {

SDL_RendererLogicalPresentation presentation_for(abuse::render::ScaleMode m)
{
    switch (m)
    {
    case abuse::render::ScaleMode::Integer: return SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
    case abuse::render::ScaleMode::Stretch: return SDL_LOGICAL_PRESENTATION_STRETCH;
    case abuse::render::ScaleMode::Fit:     break;
    }
    return SDL_LOGICAL_PRESENTATION_LETTERBOX;
}

SDL_ScaleMode scale_mode_for(abuse::render::Filter f)
{
    switch (f)
    {
    case abuse::render::Filter::Nearest: return SDL_SCALEMODE_NEAREST;
    case abuse::render::Filter::Linear:  return SDL_SCALEMODE_LINEAR;
    case abuse::render::Filter::PixelArt: break;
    }
    return SDL_SCALEMODE_PIXELART;
}

}

// The 320x200 mode is presented as 320x240 on purpose: the original pixels are
// not square, and lying about the logical height restores the intended shape.
void apply_presentation()
{
    if (renderer == NULL)
        return;

    SDL_RendererLogicalPresentation mode = presentation_for(abuse::render::options().scale);

    // Any 200 tall buffer gets the same treatment, not just the 320 wide
    // one: a widescreen picture is still made of the same non-square pixels,
    // and 427x200 presented as 427x240 is what makes it 16:9 rather than
    // 16:7.5. Phase 6, block 6.5.
    if (yres == 200)
    {
        SDL_SetRenderLogicalPresentation(renderer, xres, 240, mode);
        mouse_yscale = 200.0f / 240.0f;
    }
    else
    {
        SDL_SetRenderLogicalPresentation(renderer, xres, yres, mode);
        mouse_yscale = 1.0f;
    }

    // There is no display to sync to in a scripted run, and waiting for one
    // costs about 45% of the wall time of a replay.
    bool vsync = abuse::render::options().vsync && !abuse::harness::headless();
    SDL_SetRenderVSync(renderer, vsync ? 1 : SDL_RENDERER_VSYNC_DISABLED);
}

void apply_filter()
{
    if (texture != NULL)
        SDL_SetTextureScaleMode(texture, scale_mode_for(abuse::render::options().filter));
}

//
// set_mode()
// Set the video mode
//
void set_mode(int argc, char **argv)
{
    int win_width = xres;
    int win_height = yres;
    if (win_width < 640)
        win_width *= 2;
    if (win_height < 400)
        win_height *= 2;
    if (yres == 200)
    {
        // Correct for the weird 200 line aspect ratio: the buffer is
        // presented as if it were 240 tall.
        win_width = xres * 2;
        win_height = 480;

        // 640x480 was the whole screen in 1996 and is a postage stamp on a
        // modern display. Take the largest whole multiple that leaves room
        // for the desktop's own furniture, so the pixels stay square and the
        // window still fits: 1280x960 at 1080p, 1920x1440 at 1440p.
        SDL_DisplayID display = SDL_GetPrimaryDisplay();
        SDL_Rect usable;
        if (display && SDL_GetDisplayUsableBounds(display, &usable))
        {
            int by_width = usable.w * 9 / 10 / xres;
            int by_height = usable.h * 9 / 10 / 240;
            int multiple = by_width < by_height ? by_width : by_height;
            if (multiple > 6)
                multiple = 6;
            if (multiple > 2)
            {
                win_width = xres * multiple;
                win_height = 240 * multiple;
            }
        }
    }

    // A scripted run pins the window: the window snapshots compare what was
    // presented, and the scale mode, the filter and the letterbox all depend
    // on the shape of the window it was presented into.
    bool pinned = abuse::harness::window_size(win_width, win_height);

    // FIXME: Set the icon for this window.  Looks nice on taskbars etc.
    //SDL_WM_SetIcon(SDL_LoadBMP("abuse.bmp"), NULL);

    // HIGH_PIXEL_DENSITY lets the renderer use the full backing resolution on a
    // scaled display; the logical presentation set below keeps the game at
    // 320x200 regardless, so this only sharpens the upscale.
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (flags.fullscreen && !pinned)
        window_flags |= SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow("Abuse: Vrenna", win_width, win_height, window_flags);
    if(window == NULL)
    {
        show_startup_error("Video : Unable to create window : %s", SDL_GetError());
        exit(1);
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        show_startup_error("Video : Unable to create renderer : %s", SDL_GetError());
        exit(1);
    }
    apply_presentation();

    // Create the screen image
    main_screen = new image(ivec2(xres, yres), NULL, 2);
    if(main_screen == NULL)
    {
        // Our screen image is no good, we have to bail.
        show_startup_error("Video : Unable to create screen image.");
        exit(1);
    }
    main_screen->clear();

    // Create our 8-bit surface - this is the surface the game renders to
    surface = SDL_CreateSurface(xres, yres, SDL_PIXELFORMAT_INDEX8);
    if(surface == NULL)
    {
        // Our surface is no good, we have to bail.
        show_startup_error("Video : Unable to create 8-bit surface: %s", SDL_GetError());
        exit(1);
    }
    // And this texture is the actual texture rendered to the screen
    texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        xres, yres);
    if (texture == NULL)
    {
        show_startup_error("Video : Unable to create texture: %s", SDL_GetError());
        exit(1);
    }
    apply_filter();

    const SDL_DisplayMode* mode;
    mode = SDL_GetWindowFullscreenMode(window);
    if (mode == NULL)
    {
        // Mode can be NULL meaning "not full screen"
        printf("Video : %dx%d windowed (renderer: %s)\n", win_width, win_height,
               SDL_GetRendererName(renderer));
    }
    else
    {
        printf("Video : %dx%d %dbpp (renderer: %s)\n", mode->w, mode->h,
            SDL_BITSPERPIXEL(mode->format), SDL_GetRendererName(renderer));
    }

    // Grab and hide the mouse cursor
    SDL_HideCursor();
    if(flags.grabmouse)
        SDL_SetWindowMouseGrab(window, 1);

    update_dirty(main_screen);
}

void video_change_settings(void)
{
    SDL_SetWindowFullscreen(window, flags.fullscreen);
}

//
// close_graphics()
// Shutdown the video mode
//
void close_graphics()
{
    if(lastl)
        delete lastl;
    lastl = NULL;
    // Free our 8-bit surface
    if(surface)
        SDL_DestroySurface(surface);
    if (texture)
        SDL_DestroyTexture(texture);
    delete main_screen;
}

// put_part_image()
// Draw only dirty parts of the image
//
void put_part_image(image *im, int x, int y, int x1, int y1, int x2, int y2)
{
    int xe, ye;
    SDL_Rect srcrect, dstrect;
    int ii, jj;
    int srcx, srcy, xstep, ystep;
    Uint8 *dpixel;
    Uint16 dinset;

    if(y > yres || x > xres)
        return;

    CHECK(x1 >= 0 && x2 >= x1 && y1 >= 0 && y2 >= y1);

    // Adjust if we are trying to draw off the screen
    if(x < 0)
    {
        x1 += -x;
        x = 0;
    }
    srcrect.x = x1;
    if(x + (x2 - x1) >= xres)
        xe = xres - x + x1 - 1;
    else
        xe = x2;

    if(y < 0)
    {
        y1 += -y;
        y = 0;
    }
    srcrect.y = y1;
    if(y + (y2 - y1) >= yres)
        ye = yres - y + y1 - 1;
    else
        ye = y2;

    if(srcrect.x >= xe || srcrect.y >= ye)
        return;

    // Scale the image onto the surface
    srcrect.w = xe - srcrect.x;
    srcrect.h = ye - srcrect.y;
    dstrect.x = x;
    dstrect.y = y;
    dstrect.w = srcrect.w;
    dstrect.h = srcrect.h;

    xstep = (srcrect.w << 16) / dstrect.w;
    ystep = (srcrect.h << 16) / dstrect.h;

    srcy = ((srcrect.y) << 16);
    dinset = ((surface->w - dstrect.w)) * SDL_BYTESPERPIXEL(surface->format);

    // Lock the surface if necessary
    if(SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    dpixel = (Uint8 *)surface->pixels;
    dpixel += dstrect.x * SDL_BYTESPERPIXEL(surface->format) + (dstrect.y) * surface->pitch;

    // Update surface part
    //
    // By the pitch and not by the width: SDL pads each row of a surface to
    // an alignment, and the two are only the same number when the width
    // happens to be a multiple of four. At 320 they always were, so this
    // held for thirty years; at 427 every row lands a byte further along
    // than the last and the picture shears into a diagonal wrap.
    srcy = srcrect.y;
    dpixel = ((Uint8 *)surface->pixels) + y * surface->pitch + x ;
    for(ii=0 ; ii < srcrect.h; ii++)
    {
        memcpy(dpixel, im->scan_line(srcy) + srcrect.x , srcrect.w);
        dpixel += surface->pitch;
        srcy ++;
    }

    // Unlock the surface if we locked it.
    if(SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
}

//
// load()
// Set the palette
//
void palette::load()
{
    if(lastl)
        delete lastl;
    lastl = copy();

    // Force to only 256 colours.
    // Shouldn't be needed, but best to be safe.
    if(ncolors > 256)
        ncolors = 256;

    // Always create a palette - creates a palette that can be modified
    // In theory the same palette could (probably) be used, but that would
    // involve adding SDL code to imlib
    SDL_Palette* palette = SDL_CreateSurfacePalette(surface);
    if (palette == NULL)
    {
        printf("Video : failed to create palette! %s\n", SDL_GetError());
        return;
    }
    for(int ii = 0; ii < ncolors; ii++)
    {
        palette->colors[ii].r = red(ii);
        palette->colors[ii].g = green(ii);
        palette->colors[ii].b = blue(ii);
        palette->colors[ii].a = 255;
    }

    // Now redraw the surface
    update_window_done();
}

//
// load_nice()
//
void palette::load_nice()
{
    load();
}

// ---- support functions ----

bool save_frame_bmp(char const *path)
{
    if (surface == NULL)
        return false;
    return SDL_SaveBMP(surface, path);
}

bool game_rect_to_window(int gx, int gy, int gw, int gh,
                         int &x, int &y, int &w, int &h)
{
    if (renderer == NULL)
        return false;

    int logical_w = 0, logical_h = 0;
    SDL_RendererLogicalPresentation mode;
    if (!SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &mode)
        || logical_w < 1 || logical_h < 1)
        return false;

    SDL_FRect dst;
    if (!SDL_GetRenderLogicalPresentationRect(renderer, &dst))
        return false;

    // The game's buffer is stretched over the whole logical area, which for
    // 320x200 is 320x240: that is the aspect correction, and it applies to
    // anything measured against what the game drew.
    float sx = dst.w / (float)xres;
    float sy = dst.h / (float)yres;

    x = (int)(dst.x + gx * sx);
    y = (int)(dst.y + gy * sy);
    w = (int)(gw * sx);
    h = (int)(gh * sy);
    return true;
}

static SDL_Texture *overlay_texture = NULL;
static int overlay_tex_w = 0;
static int overlay_tex_h = 0;

bool window_pixel_size(int &w, int &h)
{
    if (renderer == NULL)
        return false;

    // SDL_GetRenderOutputSize and not SDL_GetCurrentRenderOutputSize: the
    // second one answers with the *logical* size whenever a logical
    // presentation is set, which here is 320x240. The overlay is composited
    // with that presentation turned off, so a buffer built to the logical
    // size was stretched across the window: at 1280x720 every glyph came out
    // a third wider than tall.
    return SDL_GetRenderOutputSize(renderer, &w, &h);
}

// Draws the native resolution layer over the scaled game frame. The logical
// presentation has to be off while it happens: with it on the overlay would be
// scaled like the 320x200 buffer and lose the sharpness it exists for.
// A dark line under each of the game's pixel rows, which is the visible half
// of what a CRT did to this game. Phase 6, block 6.3, without the shader
// pipeline the rest of that block needs: at output resolution, as rectangles,
// so the GPU does the work.
//
// Under the overlay on purpose. The UI is drawn at native resolution to be
// read, and striping it would undo that.
static void draw_scanlines()
{
    if (!abuse::render::options().scanlines)
        return;

    int gx, gy, gw, gh;
    if (!game_rect_to_window(0, 0, xres, yres, gx, gy, gw, gh))
        return;

    // One game row is this many window rows. Below two there is nowhere to
    // put a dark line that would not swallow the picture.
    float const row = (float)gh / (float)yres;
    if (row < 2.0f)
        return;

    std::vector<SDL_FRect> lines;
    lines.reserve((size_t)yres);

    float const thickness = row >= 4.0f ? row * 0.25f : 1.0f;
    for (int i = 0; i < yres; i++)
    {
        SDL_FRect r;
        r.x = (float)gx;
        r.w = (float)gw;
        r.y = (float)gy + (float)(i + 1) * row - thickness;
        r.h = thickness;
        lines.push_back(r);
    }

    SDL_SetRenderLogicalPresentation(renderer, 0, 0,
                                     SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_BlendMode previous;
    SDL_GetRenderDrawBlendMode(renderer, &previous);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    // Measured at 90: a tenth of the picture's light, which is enough for a
    // person to call the game too dark. A CRT did cost that and more, but it
    // was not competing with the rest of the desktop.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 60);
    SDL_RenderFillRects(renderer, lines.data(), (int)lines.size());
    SDL_SetRenderDrawBlendMode(renderer, previous);
    apply_presentation();
}

static void draw_overlay()
{
    abuse::ui::Overlay &ov = abuse::ui::overlay();
    if (!ov.Dirty())
        return;

    if (overlay_texture == NULL || overlay_tex_w != ov.Width()
        || overlay_tex_h != ov.Height())
    {
        if (overlay_texture)
            SDL_DestroyTexture(overlay_texture);
        overlay_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            ov.Width(), ov.Height());
        if (overlay_texture == NULL)
            return;
        SDL_SetTextureBlendMode(overlay_texture, SDL_BLENDMODE_BLEND);
        // One overlay pixel is one window pixel, so nothing to filter.
        SDL_SetTextureScaleMode(overlay_texture, SDL_SCALEMODE_NEAREST);
        overlay_tex_w = ov.Width();
        overlay_tex_h = ov.Height();
    }

    SDL_UpdateTexture(overlay_texture, NULL, ov.Pixels(), ov.Pitch());

    SDL_SetRenderLogicalPresentation(renderer, 0, 0,
                                     SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_RenderTexture(renderer, overlay_texture, NULL, NULL);
    apply_presentation();
}

static char g_window_capture[512] = "";

void request_window_capture(char const *path)
{
    if (!path)
        g_window_capture[0] = 0;
    else
        SDL_strlcpy(g_window_capture, path, sizeof(g_window_capture));
}

// Phase 6, block 6.2: the same conversion SDL_BlitSurface does, index to
// ARGB through the palette, with the light level applied to the real colour
// instead of having been baked into the index by a palette lookup.
//
// 64000 pixels a frame. The blit already touches every one of them; this
// adds a subtraction per channel, which is why the whole thing needs no GPU.
static void convert_lit(SDL_Surface *screen)
{
    SDL_Palette const *pal = SDL_GetSurfacePalette(surface);
    abuse::render::LightMap const &levels = abuse::render::lightmap();

    if (!pal || levels.width() < surface->w || levels.height() < surface->h)
    {
        SDL_BlitSurface(surface, NULL, screen, NULL);
        return;
    }

    for (int y = 0; y < surface->h; y++)
    {
        uint8_t const *src = (uint8_t const *)surface->pixels + y * surface->pitch;
        uint32_t *dst = (uint32_t *)((uint8_t *)screen->pixels + y * screen->pitch);
        uint8_t const *level = levels.row(y);

        for (int x = 0; x < surface->w; x++)
        {
            SDL_Color const &c = pal->colors[src[x]];
            dst[x] = abuse::render::shade(c.r, c.g, c.b, level[x]);
        }
    }
}

void update_window_done()
{
    // Convert to match the display texture
    SDL_Surface* screen;
    if (SDL_LockTextureToSurface(texture, NULL, &screen))
    {
        // Copy over to the display texture
        if (abuse::render::rgb_lighting())
            convert_lit(screen);
        else
            SDL_BlitSurface(surface, NULL, screen, NULL);
        SDL_UnlockTexture(texture);
    }
    uint8_t const *bar = abuse::render::options().letterbox;
    SDL_SetRenderDrawColor(renderer, bar[0], bar[1], bar[2], 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    draw_scanlines();
    draw_overlay();

    // Before the present: on several backends the target is no longer
    // readable once it has been presented.
    if (g_window_capture[0])
    {
        // Readback is clipped to the viewport, and the logical presentation
        // makes the viewport the letterboxed content area. Turning it off for
        // the read is what puts the bars in the picture, which is half of
        // what these captures exist to check.
        SDL_SetRenderLogicalPresentation(renderer, 0, 0,
                                         SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_Surface *shot = SDL_RenderReadPixels(renderer, NULL);
        apply_presentation();

        if (shot)
        {
            if (!SDL_SaveBMP(shot, g_window_capture))
                fprintf(stderr, "unable to write '%s': %s\n",
                        g_window_capture, SDL_GetError());
            SDL_DestroySurface(shot);
        }
        else
            fprintf(stderr, "unable to read the window back: %s\n",
                    SDL_GetError());
        g_window_capture[0] = 0;
    }

    SDL_RenderPresent(renderer);
}
