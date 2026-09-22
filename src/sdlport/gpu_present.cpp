/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See gpu_present.h.
 *
 *  This software was released into the Public Domain.
 */

#include "gpu_present.h"

#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "gpu_shaders.h"
#include "render/lightmap.h"
#include "render/options.h"

namespace abuse::sdlport::gpu {

namespace {

SDL_Window *g_window = NULL;
SDL_GPUDevice *g_device = NULL;

SDL_GPUGraphicsPipeline *g_game_pipe = NULL;     // indices into colour
SDL_GPUGraphicsPipeline *g_present_pipe = NULL;  // colour onto the window
SDL_GPUGraphicsPipeline *g_overlay_pipe = NULL;
SDL_GPUGraphicsPipeline *g_bright_pipe = NULL;   // what glows
SDL_GPUGraphicsPipeline *g_blur_pipe = NULL;     // half a Gaussian

SDL_GPUTexture *g_indexed = NULL;   // the game buffer, one byte a pixel
SDL_GPUTexture *g_palette = NULL;   // 256x1 RGBA
SDL_GPUTexture *g_overlay = NULL;   // window sized ARGB
SDL_GPUTexture *g_light = NULL;     // one light level per game pixel
SDL_GPUTexture *g_scene = NULL;     // the game picture in colour, game sized
int g_scene_w = 0, g_scene_h = 0;

// The glow, at half the scene's size: two of them, because a separable
// blur has to land somewhere between its passes.
SDL_GPUTexture *g_bloom[2] = { NULL, NULL };
int g_bloom_w = 0, g_bloom_h = 0;

// What the first pass draws into. Fixed rather than the window's format,
// so the pipeline does not have to be rebuilt when the window moves to a
// display with a different one.
SDL_GPUTextureFormat const kSceneFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

SDL_GPUSampler *g_nearest = NULL;
SDL_GPUSampler *g_linear = NULL;

SDL_GPUTransferBuffer *g_upload = NULL;
uint32_t g_upload_size = 0;

int g_game_w = 0, g_game_h = 0;
int g_overlay_w = 0, g_overlay_h = 0;

uint8_t g_palette_rgb[768];
bool g_palette_dirty = true;

char g_capture[512] = "";
SDL_GPUTexture *g_offscreen = NULL;
int g_offscreen_w = 0, g_offscreen_h = 0;
SDL_GPUTransferBuffer *g_download = NULL;
uint32_t g_download_size = 0;

SDL_GPUShader *make_shader(uint8_t const *code, size_t size,
                           SDL_GPUShaderStage stage, uint32_t samplers,
                           uint32_t uniforms = 0)
{
    SDL_GPUShaderCreateInfo ci = {};
    ci.code = code;
    ci.code_size = size;
    ci.entrypoint = "main";
    ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
    ci.stage = stage;
    ci.num_samplers = samplers;
    ci.num_uniform_buffers = uniforms;
    return SDL_CreateGPUShader(g_device, &ci);
}

SDL_GPUGraphicsPipeline *make_pipeline(SDL_GPUShader *vs, SDL_GPUShader *fs,
                                       bool blend,
                                       SDL_GPUTextureFormat format)
{
    SDL_GPUColorTargetDescription target = {};
    target.format = format;

    if (blend)
    {
        // The overlay carries straight alpha, the way the window manager
        // composites it on the old path.
        target.blend_state.enable_blend = true;
        target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        target.blend_state.dst_color_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        target.blend_state.dst_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }

    SDL_GPUGraphicsPipelineCreateInfo ci = {};
    ci.vertex_shader = vs;
    ci.fragment_shader = fs;
    ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    ci.target_info.num_color_targets = 1;
    ci.target_info.color_target_descriptions = &target;
    return SDL_CreateGPUGraphicsPipeline(g_device, &ci);
}

SDL_GPUSampler *make_sampler(SDL_GPUFilter filter)
{
    SDL_GPUSamplerCreateInfo ci = {};
    ci.min_filter = ci.mag_filter = filter;
    ci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    ci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    ci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    return SDL_CreateGPUSampler(g_device, &ci);
}

SDL_GPUTexture *make_texture(int w, int h, SDL_GPUTextureFormat format)
{
    SDL_GPUTextureCreateInfo ci = {};
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = format;
    ci.width = (uint32_t)w;
    ci.height = (uint32_t)h;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    return SDL_CreateGPUTexture(g_device, &ci);
}

// What the present shader reads, laid out as its uniform block is.
// What the first pass reads, laid out as its uniform block is.
//
// Padded to sixteen bytes. A uniform block is laid out in multiples of a
// four-component vector, and pushing only the four bytes the one member
// needs left the shader reading whatever was there: the lighting branch
// never ran, and the frame came out identical to one with the lighting
// switched off, which took a while to tell apart from a missing light
// map.
struct SceneUniform
{
    float lit;
    float pad[3];
};

struct PresentUniform
{
    float source_w;
    float source_h;
    float scanline;
    float mode;
    float glow;
    float pad[3];
};

struct BrightUniform
{
    float threshold;
    float pad[3];
};

struct BlurUniform
{
    float step_x;
    float step_y;
    float pad[2];
};

// Grows the staging buffer when a frame needs more than the last one did.
bool want_upload(uint32_t bytes)
{
    if (g_upload && g_upload_size >= bytes)
        return true;

    if (g_upload)
        SDL_ReleaseGPUTransferBuffer(g_device, g_upload);

    SDL_GPUTransferBufferCreateInfo ci = {};
    ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ci.size = bytes;
    g_upload = SDL_CreateGPUTransferBuffer(g_device, &ci);
    g_upload_size = g_upload ? bytes : 0;
    return g_upload != NULL;
}

// Where the game picture sits inside the window.
//
// The buffer is 200 rows and is shown as though it were 240, which is the
// correction the 1995 mode needs and what the old path asks
// SDL_SetRenderLogicalPresentation for.
void fit(int win_w, int win_h, int game_w, int game_h,
         int &x, int &y, int &w, int &h)
{
    int const tall = game_h == 200 ? 240 : game_h;

    switch (render::options().scale)
    {
    case render::ScaleMode::Stretch:
        x = y = 0;
        w = win_w;
        h = win_h;
        return;

    case render::ScaleMode::Integer:
    {
        int by_w = win_w / game_w;
        int by_h = win_h / tall;
        int m = by_w < by_h ? by_w : by_h;
        if (m < 1)
            m = 1;
        w = game_w * m;
        h = tall * m;
        break;
    }

    case render::ScaleMode::Fit:
    default:
        if (win_w * tall <= win_h * game_w)
        {
            w = win_w;
            h = win_w * tall / game_w;
        }
        else
        {
            h = win_h;
            w = win_h * game_w / tall;
        }
        break;
    }

    x = (win_w - w) / 2;
    y = (win_h - h) / 2;
}

// Reads the finished frame back and writes it out.
//
// Through a render target of its own rather than the swapchain: a
// swapchain texture is not required to be downloadable, and on some
// drivers it is not.
void save_capture(SDL_GPUTexture *from, uint32_t w, uint32_t h,
                  SDL_GPUTextureFormat format)
{
    uint32_t const bytes = w * h * 4;
    if (!g_download || g_download_size < bytes)
    {
        if (g_download)
            SDL_ReleaseGPUTransferBuffer(g_device, g_download);
        SDL_GPUTransferBufferCreateInfo ci = {};
        ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        ci.size = bytes;
        g_download = SDL_CreateGPUTransferBuffer(g_device, &ci);
        g_download_size = g_download ? bytes : 0;
    }
    if (!g_download)
        return;

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(g_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion src = {};
    src.texture = from;
    src.w = w;
    src.h = h;
    src.d = 1;
    SDL_GPUTextureTransferInfo dst = {};
    dst.transfer_buffer = g_download;
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence)
    {
        SDL_WaitForGPUFences(g_device, true, &fence, 1);
        SDL_ReleaseGPUFence(g_device, fence);
    }

    void *pixels = SDL_MapGPUTransferBuffer(g_device, g_download, false);
    if (pixels)
    {
        SDL_PixelFormat const sdl_format =
            format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
                ? SDL_PIXELFORMAT_ABGR8888 : SDL_PIXELFORMAT_ARGB8888;
        SDL_Surface *shot = SDL_CreateSurfaceFrom((int)w, (int)h, sdl_format,
                                                  pixels, (int)(w * 4));
        if (shot)
        {
            if (!SDL_SaveBMP(shot, g_capture))
                fprintf(stderr, "unable to write '%s': %s\n",
                        g_capture, SDL_GetError());
            SDL_DestroySurface(shot);
        }
        SDL_UnmapGPUTransferBuffer(g_device, g_download);
    }
    g_capture[0] = 0;
}

}

bool window_size(int &w, int &h)
{
    if (!g_device || !g_window)
        return false;
    return SDL_GetWindowSizeInPixels(g_window, &w, &h);
}

bool picture_rect(int game_w, int game_h, int &x, int &y, int &w, int &h)
{
    int win_w = 0, win_h = 0;
    if (!window_size(win_w, win_h) || game_w < 1 || game_h < 1)
        return false;

    fit(win_w, win_h, game_w, game_h, x, y, w, h);
    return true;
}

void capture_next_frame(char const *path)
{
    if (path)
        SDL_strlcpy(g_capture, path, sizeof g_capture);
}

bool running()
{
    return g_device != NULL;
}

bool start(SDL_Window *window)
{
    if (g_device)
        return true;

    g_window = window;
    g_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (!g_device)
    {
        printf("GPU: no device that takes SPIR-V (%s); presenting the old "
               "way\n", SDL_GetError());
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(g_device, window))
    {
        printf("GPU: could not claim the window (%s)\n", SDL_GetError());
        stop();
        return false;
    }

    SDL_GPUShader *vs = make_shader(render::gpu::k_palette_vert,
                                    sizeof render::gpu::k_palette_vert,
                                    SDL_GPU_SHADERSTAGE_VERTEX, 0);
    SDL_GPUShader *fs_game = make_shader(render::gpu::k_palette_frag,
                                         sizeof render::gpu::k_palette_frag,
                                         SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 1);
    SDL_GPUShader *fs_over = make_shader(render::gpu::k_overlay_frag,
                                         sizeof render::gpu::k_overlay_frag,
                                         SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
    SDL_GPUShader *fs_present = make_shader(render::gpu::k_present_frag,
                                            sizeof render::gpu::k_present_frag,
                                            SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 1);
    SDL_GPUShader *fs_bright = make_shader(render::gpu::k_bright_frag,
                                           sizeof render::gpu::k_bright_frag,
                                           SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    SDL_GPUShader *fs_blur = make_shader(render::gpu::k_blur_frag,
                                         sizeof render::gpu::k_blur_frag,
                                         SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    if (!vs || !fs_game || !fs_over || !fs_present || !fs_bright || !fs_blur)
    {
        printf("GPU: shader rejected (%s)\n", SDL_GetError());
        stop();
        return false;
    }

    // The first pass draws into a target of the game's own size, so its
    // format is fixed here rather than taken from the window.
    g_game_pipe = make_pipeline(vs, fs_game, false, kSceneFormat);
    g_present_pipe = make_pipeline(vs, fs_present, false,
                                   SDL_GetGPUSwapchainTextureFormat(g_device,
                                                                    window));
    g_overlay_pipe = make_pipeline(vs, fs_over, true,
                                   SDL_GetGPUSwapchainTextureFormat(g_device,
                                                                    window));

    SDL_ReleaseGPUShader(g_device, vs);
    SDL_ReleaseGPUShader(g_device, fs_game);
    SDL_ReleaseGPUShader(g_device, fs_over);
    g_bright_pipe = make_pipeline(vs, fs_bright, false, kSceneFormat);
    g_blur_pipe = make_pipeline(vs, fs_blur, false, kSceneFormat);

    SDL_ReleaseGPUShader(g_device, fs_present);
    SDL_ReleaseGPUShader(g_device, fs_bright);
    SDL_ReleaseGPUShader(g_device, fs_blur);

    g_nearest = make_sampler(SDL_GPU_FILTER_NEAREST);
    g_linear = make_sampler(SDL_GPU_FILTER_LINEAR);
    g_palette = make_texture(256, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);

    if (!g_game_pipe || !g_present_pipe || !g_overlay_pipe || !g_bright_pipe
        || !g_blur_pipe || !g_nearest || !g_linear || !g_palette)
    {
        printf("GPU: pipeline setup failed (%s)\n", SDL_GetError());
        stop();
        return false;
    }

    g_palette_dirty = true;
    printf("Video : presenting through SDL_GPU (%s)\n",
           SDL_GetGPUDeviceDriver(g_device));
    return true;
}

void stop()
{
    if (!g_device)
        return;

    SDL_WaitForGPUIdle(g_device);

    if (g_upload) SDL_ReleaseGPUTransferBuffer(g_device, g_upload);
    if (g_download) SDL_ReleaseGPUTransferBuffer(g_device, g_download);
    if (g_offscreen) SDL_ReleaseGPUTexture(g_device, g_offscreen);
    if (g_indexed) SDL_ReleaseGPUTexture(g_device, g_indexed);
    if (g_palette) SDL_ReleaseGPUTexture(g_device, g_palette);
    if (g_overlay) SDL_ReleaseGPUTexture(g_device, g_overlay);
    if (g_nearest) SDL_ReleaseGPUSampler(g_device, g_nearest);
    if (g_linear) SDL_ReleaseGPUSampler(g_device, g_linear);
    if (g_light) SDL_ReleaseGPUTexture(g_device, g_light);
    if (g_scene) SDL_ReleaseGPUTexture(g_device, g_scene);
    if (g_game_pipe) SDL_ReleaseGPUGraphicsPipeline(g_device, g_game_pipe);
    if (g_present_pipe) SDL_ReleaseGPUGraphicsPipeline(g_device, g_present_pipe);
    if (g_bright_pipe) SDL_ReleaseGPUGraphicsPipeline(g_device, g_bright_pipe);
    if (g_blur_pipe) SDL_ReleaseGPUGraphicsPipeline(g_device, g_blur_pipe);
    for (int i = 0; i < 2; i++)
        if (g_bloom[i]) SDL_ReleaseGPUTexture(g_device, g_bloom[i]);
    if (g_overlay_pipe) SDL_ReleaseGPUGraphicsPipeline(g_device, g_overlay_pipe);
    if (g_window) SDL_ReleaseWindowFromGPUDevice(g_device, g_window);

    SDL_DestroyGPUDevice(g_device);

    g_upload = NULL; g_upload_size = 0;
    g_download = NULL; g_download_size = 0;
    g_offscreen = NULL; g_offscreen_w = g_offscreen_h = 0;
    g_indexed = g_palette = g_overlay = NULL;
    g_nearest = g_linear = NULL;
    g_light = NULL;
    g_scene = NULL; g_scene_w = g_scene_h = 0;
    g_bloom[0] = g_bloom[1] = NULL; g_bloom_w = g_bloom_h = 0;
    g_game_pipe = g_present_pipe = g_overlay_pipe = NULL;
    g_bright_pipe = g_blur_pipe = NULL;
    g_device = NULL;
    g_window = NULL;
    g_game_w = g_game_h = g_overlay_w = g_overlay_h = 0;
}

void set_palette(uint8_t const rgb[768])
{
    if (memcmp(g_palette_rgb, rgb, sizeof g_palette_rgb) == 0)
        return;
    memcpy(g_palette_rgb, rgb, sizeof g_palette_rgb);
    g_palette_dirty = true;
}

void present(uint8_t const *indexed, int w, int h, int pitch,
             uint32_t const *overlay, int ow, int oh, int opitch)
{
    if (!g_device || !indexed || w <= 0 || h <= 0)
        return;

    if (w != g_game_w || h != g_game_h)
    {
        if (g_indexed)
            SDL_ReleaseGPUTexture(g_device, g_indexed);
        g_indexed = make_texture(w, h, SDL_GPU_TEXTUREFORMAT_R8_UNORM);

        if (g_light)
            SDL_ReleaseGPUTexture(g_device, g_light);
        // Four channels since block 6.2 grew a level per channel: a
        // coloured light takes less off the channels it is made of.
        g_light = make_texture(w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);

        g_game_w = w;
        g_game_h = h;
    }

    // The colour picture at the game's own size: what the first pass
    // writes and the second one scales.
    if (w != g_scene_w || h != g_scene_h)
    {
        if (g_scene)
            SDL_ReleaseGPUTexture(g_device, g_scene);

        SDL_GPUTextureCreateInfo ci = {};
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = kSceneFormat;
        ci.width = (uint32_t)w;
        ci.height = (uint32_t)h;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
                   | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        g_scene = SDL_CreateGPUTexture(g_device, &ci);
        g_scene_w = w;
        g_scene_h = h;
    }

    bool const want_overlay = overlay && ow > 0 && oh > 0;
    if (want_overlay && (ow != g_overlay_w || oh != g_overlay_h))
    {
        if (g_overlay)
            SDL_ReleaseGPUTexture(g_device, g_overlay);
        g_overlay = make_texture(ow, oh, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
        g_overlay_w = ow;
        g_overlay_h = oh;
    }

    if (!g_indexed || !g_light || !g_scene || (want_overlay && !g_overlay))
        return;

    // One staging buffer for everything this frame: the game rows, the
    // palette when it moved, and the overlay when there is one.
    // The light map only goes up when it is being used and when it covers
    // the frame. When it does not, the lighting was already applied by
    // remapping indices and there is nothing here to apply.
    render::LightMap const &levels = render::lightmap();
    bool const lit = render::rgb_lighting()
                     && levels.width() >= w && levels.height() >= h;

    uint32_t const game_bytes = (uint32_t)(w * h);
    uint32_t const light_bytes = lit ? (uint32_t)(w * h * 4) : 0u;
    uint32_t const pal_bytes = g_palette_dirty ? 256u * 4u : 0u;
    uint32_t const over_bytes = want_overlay ? (uint32_t)(ow * oh * 4) : 0u;
    if (!want_upload(game_bytes + light_bytes + pal_bytes + over_bytes))
        return;

    uint8_t *m = (uint8_t *)SDL_MapGPUTransferBuffer(g_device, g_upload, true);
    if (!m)
        return;

    // Row by row, because the source is padded and the destination is not.
    for (int y = 0; y < h; y++)
        memcpy(m + (size_t)y * w, indexed + (size_t)y * pitch, (size_t)w);

    if (light_bytes)
    {
        // Three bytes per pixel on this side, four on that one.
        uint8_t *l = m + game_bytes;
        for (int y = 0; y < h; y++)
        {
            uint8_t const *src = levels.row(y);
            uint8_t *dst = l + (size_t)y * (size_t)w * 4u;
            for (int x = 0; x < w; x++)
            {
                dst[x * 4 + 0] = src[x * 3 + 0];
                dst[x * 4 + 1] = src[x * 3 + 1];
                dst[x * 4 + 2] = src[x * 3 + 2];
                dst[x * 4 + 3] = 255;
            }
        }
    }

    if (pal_bytes)
    {
        uint8_t *p = m + game_bytes + light_bytes;
        for (int i = 0; i < 256; i++)
        {
            p[i * 4 + 0] = g_palette_rgb[i * 3 + 0];
            p[i * 4 + 1] = g_palette_rgb[i * 3 + 1];
            p[i * 4 + 2] = g_palette_rgb[i * 3 + 2];
            p[i * 4 + 3] = 255;
        }
    }

    if (over_bytes)
    {
        uint8_t *o = m + game_bytes + light_bytes + pal_bytes;
        for (int y = 0; y < oh; y++)
            memcpy(o + (size_t)y * ow * 4,
                   (uint8_t const *)overlay + (size_t)y * opitch,
                   (size_t)ow * 4);
    }

    SDL_UnmapGPUTransferBuffer(g_device, g_upload);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(g_device);
    if (!cmd)
        return;

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo from = {};
    from.transfer_buffer = g_upload;
    SDL_GPUTextureRegion to = {};
    to.d = 1;

    from.offset = 0;
    to.texture = g_indexed;
    to.w = (uint32_t)w;
    to.h = (uint32_t)h;
    SDL_UploadToGPUTexture(copy, &from, &to, true);

    if (light_bytes)
    {
        from.offset = game_bytes;
        to.texture = g_light;
        to.w = (uint32_t)w;
        to.h = (uint32_t)h;
        SDL_UploadToGPUTexture(copy, &from, &to, true);
    }

    if (pal_bytes)
    {
        from.offset = game_bytes + light_bytes;
        to.texture = g_palette;
        to.w = 256;
        to.h = 1;
        SDL_UploadToGPUTexture(copy, &from, &to, true);
        g_palette_dirty = false;
    }

    if (over_bytes)
    {
        from.offset = game_bytes + light_bytes + pal_bytes;
        to.texture = g_overlay;
        to.w = (uint32_t)ow;
        to.h = (uint32_t)oh;
        SDL_UploadToGPUTexture(copy, &from, &to, true);
    }

    SDL_EndGPUCopyPass(copy);

    SDL_GPUTexture *swap = NULL;
    uint32_t sw = 0, sh = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, g_window, &swap, &sw, &sh)
        || !swap)
    {
        // The window is minimised or being resized. Nothing to present, and
        // the buffer still has to go somewhere.
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }

    // With a capture pending the scene goes to a target of its own first,
    // which is then blitted to the window and downloaded. A swapchain
    // texture is not required to be downloadable.
    bool const capturing = g_capture[0] != 0;
    SDL_GPUTextureFormat const swap_format =
        SDL_GetGPUSwapchainTextureFormat(g_device, g_window);

    if (capturing && ((int)sw != g_offscreen_w || (int)sh != g_offscreen_h))
    {
        if (g_offscreen)
            SDL_ReleaseGPUTexture(g_device, g_offscreen);
        SDL_GPUTextureCreateInfo ci = {};
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = swap_format;
        ci.width = sw;
        ci.height = sh;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
                   | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        g_offscreen = SDL_CreateGPUTexture(g_device, &ci);
        g_offscreen_w = (int)sw;
        g_offscreen_h = (int)sh;
    }

    // Pass one: indices into colour, at the game's own size.
    //
    // Both samplers are nearest and neither is ever anything else.
    // Interpolating an index averages the numbers, not the colours:
    // halfway between entry 3 and entry 200 is entry 101, which resembles
    // neither. The first frame this path produced was black with bright
    // orange edges for exactly that reason. Smoothing belongs to the
    // second pass, where the values are colours.
    SDL_GPUColorTargetInfo scene_target = {};
    scene_target.texture = g_scene;
    scene_target.load_op = SDL_GPU_LOADOP_DONT_CARE;
    scene_target.store_op = SDL_GPU_STOREOP_STORE;

    SceneUniform su = {};
    su.lit = lit ? 1.0f : 0.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &su, sizeof su);

    SDL_GPURenderPass *scene_pass =
        SDL_BeginGPURenderPass(cmd, &scene_target, 1, NULL);
    SDL_BindGPUGraphicsPipeline(scene_pass, g_game_pipe);
    SDL_GPUTextureSamplerBinding game_bind[3] = {};
    game_bind[0].texture = g_indexed;
    game_bind[0].sampler = g_nearest;
    game_bind[1].texture = g_palette;
    game_bind[1].sampler = g_nearest;
    game_bind[2].texture = g_light;
    game_bind[2].sampler = g_nearest;
    SDL_BindGPUFragmentSamplers(scene_pass, 0, game_bind, 3);
    SDL_DrawGPUPrimitives(scene_pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(scene_pass);

    // The glow, when it is wanted: what is bright, blurred, at half size.
    //
    // Half size is both cheaper and the first half of the blur, since the
    // hardware averages four texels on the way down. Two targets because
    // a separable blur has to land somewhere between across and down.
    render::Options const &pre = render::options();
    bool const glowing = pre.bloom > 0.0f;

    if (glowing)
    {
        int const bw = w / 2 > 0 ? w / 2 : 1;
        int const bh = h / 2 > 0 ? h / 2 : 1;

        if (bw != g_bloom_w || bh != g_bloom_h)
        {
            for (int i = 0; i < 2; i++)
            {
                if (g_bloom[i])
                    SDL_ReleaseGPUTexture(g_device, g_bloom[i]);

                SDL_GPUTextureCreateInfo ci = {};
                ci.type = SDL_GPU_TEXTURETYPE_2D;
                ci.format = kSceneFormat;
                ci.width = (uint32_t)bw;
                ci.height = (uint32_t)bh;
                ci.layer_count_or_depth = 1;
                ci.num_levels = 1;
                ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
                           | SDL_GPU_TEXTUREUSAGE_SAMPLER;
                g_bloom[i] = SDL_CreateGPUTexture(g_device, &ci);
            }
            g_bloom_w = bw;
            g_bloom_h = bh;
        }
    }

    if (glowing && g_bloom[0] && g_bloom[1])
    {
        BrightUniform bu = {};
        bu.threshold = pre.bloom_threshold;
        SDL_PushGPUFragmentUniformData(cmd, 0, &bu, sizeof bu);

        SDL_GPUColorTargetInfo bt = {};
        bt.texture = g_bloom[0];
        bt.load_op = SDL_GPU_LOADOP_DONT_CARE;
        bt.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *bp = SDL_BeginGPURenderPass(cmd, &bt, 1, NULL);
        SDL_BindGPUGraphicsPipeline(bp, g_bright_pipe);
        SDL_GPUTextureSamplerBinding b = {};
        b.texture = g_scene;
        b.sampler = g_linear;
        SDL_BindGPUFragmentSamplers(bp, 0, &b, 1);
        SDL_DrawGPUPrimitives(bp, 3, 1, 0, 0);
        SDL_EndGPURenderPass(bp);

        // Across into [1], then down back into [0].
        for (int dir = 0; dir < 2; dir++)
        {
            BlurUniform blu = {};
            blu.step_x = dir == 0 ? 1.0f / (float)g_bloom_w : 0.0f;
            blu.step_y = dir == 0 ? 0.0f : 1.0f / (float)g_bloom_h;
            SDL_PushGPUFragmentUniformData(cmd, 0, &blu, sizeof blu);

            SDL_GPUColorTargetInfo t = {};
            t.texture = g_bloom[dir == 0 ? 1 : 0];
            t.load_op = SDL_GPU_LOADOP_DONT_CARE;
            t.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass *pass2 = SDL_BeginGPURenderPass(cmd, &t, 1, NULL);
            SDL_BindGPUGraphicsPipeline(pass2, g_blur_pipe);
            SDL_GPUTextureSamplerBinding src = {};
            src.texture = g_bloom[dir == 0 ? 0 : 1];
            src.sampler = g_linear;
            SDL_BindGPUFragmentSamplers(pass2, 0, &src, 1);
            SDL_DrawGPUPrimitives(pass2, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass2);
        }
    }

    // Pass two: that picture, scaled into the window.
    uint8_t const *bar = render::options().letterbox;
    SDL_GPUColorTargetInfo target = {};
    target.texture = (capturing && g_offscreen) ? g_offscreen : swap;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    target.clear_color.r = bar[0] / 255.0f;
    target.clear_color.g = bar[1] / 255.0f;
    target.clear_color.b = bar[2] / 255.0f;
    target.clear_color.a = 1.0f;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);

    int gx, gy, gw, gh;
    fit((int)sw, (int)sh, w, h, gx, gy, gw, gh);

    SDL_GPUViewport view = {};
    view.x = (float)gx;
    view.y = (float)gy;
    view.w = (float)gw;
    view.h = (float)gh;
    view.max_depth = 1.0f;
    SDL_SetGPUViewport(pass, &view);

    render::Options const &opt = render::options();

    PresentUniform u = {};
    u.source_w = (float)w;
    u.source_h = (float)h;
    // Only where there is room for them, as on the old path: fewer than
    // two window rows per game row and a scanline is most of the picture.
    u.scanline = (opt.scanlines && gh >= h * 2) ? 0.35f : 0.0f;
    // 0 plain, 1 sharp bilinear, 2 Scale2x. Must match present.frag.glsl.
    u.mode = opt.filter == render::Filter::Scale2x    ? 2.0f
             : opt.filter == render::Filter::PixelArt ? 1.0f
                                                      : 0.0f;
    u.glow = (glowing && g_bloom[0]) ? opt.bloom : 0.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &u, sizeof u);

    SDL_BindGPUGraphicsPipeline(pass, g_present_pipe);
    SDL_GPUTextureSamplerBinding scene_bind = {};
    scene_bind.texture = g_scene;
    // Nearest keeps hard edges but makes them uneven at a fractional
    // scale; the sharpening in the shader needs a linear sampler to
    // blend between two texels at all.
    // Scale2x compares texels and must see them unblended, like nearest.
    scene_bind.sampler = (opt.filter == render::Filter::Nearest
                          || opt.filter == render::Filter::Scale2x)
                             ? g_nearest : g_linear;
    SDL_GPUTextureSamplerBinding present_bind[2] = {};
    present_bind[0] = scene_bind;
    // Always bound, even with no glow: a pipeline asks for its samplers
    // whether the shader reads them or not.
    present_bind[1].texture = g_bloom[0] ? g_bloom[0] : g_scene;
    present_bind[1].sampler = g_linear;
    SDL_BindGPUFragmentSamplers(pass, 0, present_bind, 2);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);

    if (over_bytes)
    {
        // Over the whole window and not only the picture: the overlay is
        // already at window resolution, which is the point of it.
        SDL_GPUViewport full = {};
        full.w = (float)sw;
        full.h = (float)sh;
        full.max_depth = 1.0f;
        SDL_SetGPUViewport(pass, &full);

        SDL_BindGPUGraphicsPipeline(pass, g_overlay_pipe);
        SDL_GPUTextureSamplerBinding over_bind = {};
        over_bind.texture = g_overlay;
        over_bind.sampler = g_nearest;
        SDL_BindGPUFragmentSamplers(pass, 0, &over_bind, 1);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }

    SDL_EndGPURenderPass(pass);

    if (capturing && g_offscreen)
    {
        SDL_GPUBlitInfo blit = {};
        blit.source.texture = g_offscreen;
        blit.source.w = sw;
        blit.source.h = sh;
        blit.destination.texture = swap;
        blit.destination.w = sw;
        blit.destination.h = sh;
        blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(cmd, &blit);
    }

    SDL_SubmitGPUCommandBuffer(cmd);

    if (capturing && g_offscreen)
        save_capture(g_offscreen, sw, sh, swap_format);
}

}
