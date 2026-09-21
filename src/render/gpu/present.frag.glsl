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
layout(set = 2, binding = 1) uniform sampler2D u_bloom;

layout(set = 3, binding = 0) uniform Present
{
    // The scene texture, in texels. Needed to reason in source pixels.
    vec2 u_source;

    // How much of each source row the scanline eats, 0 for none.
    float u_scanline;

    // 0 takes the sampler as it is, 1 sharpens the interpolation,
    // 2 runs Scale2x.
    float u_mode;

    // How much of the blurred bright pass to add back, 0 for none.
    float u_glow;
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

// Scale2x, the 1990s pixel-art doubler, as a per-output-pixel lookup.
//
// The original algorithm walks the source and writes four pixels; here
// each output pixel works out which of those four it would have been and
// computes only that one. Same rule, no intermediate buffer.
//
// A corner is only filled in when the two neighbours meeting there agree
// and the two opposite pairs disagree, which is what turns a staircase
// into a diagonal and leaves a deliberate right angle alone.
vec3 scale2x(vec2 uv)
{
    vec2 texel = 1.0 / u_source;
    vec2 pixel = uv * u_source;
    vec2 within = fract(pixel);

    vec3 e = texture(u_scene, uv).rgb;
    vec3 b = texture(u_scene, uv + vec2(0.0, -texel.y)).rgb;
    vec3 h = texture(u_scene, uv + vec2(0.0,  texel.y)).rgb;
    vec3 d = texture(u_scene, uv + vec2(-texel.x, 0.0)).rgb;
    vec3 f = texture(u_scene, uv + vec2( texel.x, 0.0)).rgb;

    // Flat, or an edge running both ways: leave it alone.
    if (b == h || d == f)
        return e;

    bool left = within.x < 0.5;
    bool top = within.y < 0.5;

    if (top)
        return left ? (d == b ? d : e) : (b == f ? f : e);
    return left ? (d == h ? d : e) : (h == f ? f : e);
}

void main()
{
    vec3 colour;
    if (u_mode > 1.5)
        colour = scale2x(v_uv);
    else
        colour = texture(u_scene, u_mode > 0.5 ? sharpen(v_uv) : v_uv).rgb;

    if (u_glow > 0.0)
    {
        // Added rather than blended: a glow is light arriving on top of
        // what is already there, and the bright pass already carries how
        // much there is.
        //
        // Sampled at the unsharpened coordinate on purpose. The bloom is
        // a blur; putting it through the pixel-art correction would be
        // asking for hard edges on the one thing that has none.
        colour += texture(u_bloom, v_uv).rgb * u_glow;
    }

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
