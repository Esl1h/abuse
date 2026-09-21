#version 450

// The native-resolution overlay, composited over the game picture.
//
// It arrives as straight ARGB at window size, already the right shape, so
// there is nothing to convert: sample it and let the blend state do the
// rest. It exists as a shader of its own only because the palette one
// takes an index and this one takes a colour.

layout(set = 2, binding = 0) uniform sampler2D u_overlay;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

void main()
{
    o_colour = texture(u_overlay, v_uv);
}
