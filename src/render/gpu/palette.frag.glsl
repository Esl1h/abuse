#version 450

// The 1995 framebuffer, converted on the GPU.
//
// The game draws into an 8-bit indexed buffer and has done since 1995. Up
// to now the conversion to colour happened on the CPU, once per pixel per
// frame: at 1080p that is two million lookups a frame and it is why the
// plan calls the CPU no use for this.
//
// Here the indexed buffer arrives as an R8 texture and the palette as a
// 256x1 RGBA one, and the lookup is a second sample.
//
// SDL_GPU on Vulkan puts fragment samplers in descriptor set 2 and
// fragment uniform buffers in set 3. The order they are declared in is the
// order they are bound in.

layout(set = 2, binding = 0) uniform sampler2D u_indexed;
layout(set = 2, binding = 1) uniform sampler2D u_palette;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

void main()
{
    // R8 arrives normalised to 0..1, so the index is that times 255. The
    // half-texel offset lands in the middle of the palette entry rather
    // than on the boundary between two.
    float index = texture(u_indexed, v_uv).r;
    float slot = (index * 255.0 + 0.5) / 256.0;

    o_colour = vec4(texture(u_palette, vec2(slot, 0.5)).rgb, 1.0);
}
