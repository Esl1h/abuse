#version 450

// Half of a separable Gaussian. Run twice, once across and once down.
//
// Nine taps with linear sampling between pairs, which is the usual trick:
// each fetch lands between two texels and the hardware returns their
// weighted average, so nine taps cost five fetches.

layout(set = 2, binding = 0) uniform sampler2D u_source;

layout(set = 3, binding = 0) uniform Blur
{
    // One texel, in the direction being blurred. The other component is
    // zero, which is what makes the same shader do both passes.
    vec2 u_step;
};

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

void main()
{
    // Offsets and weights for a 9-tap Gaussian folded into 5 fetches.
    const float offsets[3] = float[3](0.0, 1.3846153846, 3.2307692308);
    const float weights[3] = float[3](0.2270270270, 0.3162162162, 0.0702702703);

    vec3 sum = texture(u_source, v_uv).rgb * weights[0];
    for (int i = 1; i < 3; i++)
    {
        vec2 off = u_step * offsets[i];
        sum += texture(u_source, v_uv + off).rgb * weights[i];
        sum += texture(u_source, v_uv - off).rgb * weights[i];
    }

    o_colour = vec4(sum, 1.0);
}
