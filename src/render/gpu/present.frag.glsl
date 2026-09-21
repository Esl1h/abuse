#version 450

// The second pass: the finished game picture, scaled to the window.
//
// The first pass turns indices into colour at game resolution. Everything
// that has to happen to colours rather than to indices happens here, which
// is the whole reason for the split: interpolating an index averages the
// numbers and lands on a palette entry that resembles neither of its
// neighbours.
//
// SDL_GPU on Vulkan puts fragment samplers in descriptor set 2 and
// fragment uniform buffers in set 3.

layout(set = 2, binding = 0) uniform sampler2D u_scene;

layout(set = 3, binding = 0) uniform Present
{
    // The scene texture, in texels. Needed to reason in source pixels.
    vec2 u_source;

    // How much of each source row the scanline eats, 0 for none.
    float u_scanline;

    // 1 to sharpen the interpolation, 0 to take the sampler as it is.
    float u_pixel_art;
};

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

// Sharp bilinear: linear filtering confined to the boundary between two
// texels instead of spread across the whole of one.
//
// A plain linear upscale of a 320-wide picture to 1280 blurs every edge
// over four output pixels. Nearest keeps the edges hard but makes them
// uneven whenever the factor is not a whole number, because some source
// pixels get one more output pixel than their neighbours. This keeps the
// flat parts flat and blends only where two texels actually meet, which is
// what the Filter::PixelArt setting has always promised.
vec2 sharpen(vec2 uv)
{
    vec2 texel = uv * u_source;
    vec2 base = floor(texel);
    vec2 within = texel - base;

    // Output pixels per source texel. fwidth gives the reciprocal, texels
    // per output pixel; below one the picture is being shrunk and there
    // is nothing to sharpen.
    vec2 scale = max(1.0 / max(fwidth(texel), vec2(1e-5)), vec2(1.0));

    // The flat middle of a texel, where no blending should happen at all.
    // At a scale of four, that is the central three quarters.
    vec2 region = 0.5 - 0.5 / scale;

    // Zero inside that middle, and only outside it does the distance from
    // the centre get stretched towards the neighbour.
    vec2 offset = within - 0.5;
    vec2 moved = (offset - clamp(offset, -region, region)) * scale + 0.5;

    return (base + moved) / u_source;
}

void main()
{
    vec2 uv = u_pixel_art > 0.5 ? sharpen(v_uv) : v_uv;
    vec3 colour = texture(u_scene, uv).rgb;

    if (u_scanline > 0.0)
    {
        // A dark band along the lower part of each source row, the way a
        // CRT left one. Softened rather than switched, so it does not
        // shimmer when the scale factor is not a whole number.
        float within = fract(v_uv.y * u_source.y);
        colour *= 1.0 - u_scanline * smoothstep(0.35, 1.0, within);
    }

    o_colour = vec4(colour, 1.0);
}
