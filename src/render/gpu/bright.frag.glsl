#version 450

// What glows: the part of each pixel that is above the threshold.
//
// Drawn into a target at half the game's size, which is both cheaper and
// the first half of the blur: the hardware's own filtering averages four
// texels on the way down.
//
// The plan asked for bloom driven by the light buffer of block 6.2. This
// takes luminance instead, and on purpose. The light buffer says how lit a
// pixel is, not whether anything there is bright: a white wall in full
// light and a lamp in full light are the same number to it, and only one of
// them should glow.

layout(set = 2, binding = 0) uniform sampler2D u_scene;

layout(set = 3, binding = 0) uniform Bright
{
    // Below this nothing glows. In luminance, 0 to 1.
    float u_threshold;
};

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

void main()
{
    vec3 colour = texture(u_scene, v_uv).rgb;

    // Rec. 601, which is the weighting the eye gives these channels and
    // the one the rest of this engine's palette work assumes.
    float luma = dot(colour, vec3(0.299, 0.587, 0.114));

    // Scaled by how far past the threshold it is rather than switched on
    // at it, so a pixel that drifts across the line does not pop.
    float over = max(luma - u_threshold, 0.0);
    float weight = over / max(1.0 - u_threshold, 1e-4);

    o_colour = vec4(colour * weight, 1.0);
}
