#version 450

// A full-screen triangle with no vertex buffer at all: three vertices, the
// positions worked out from the index. Cheaper than a quad and it has no
// seam down the diagonal.
//
// SDL_GPU on Vulkan puts vertex uniform buffers in descriptor set 1; this
// shader needs none.

layout(location = 0) out vec2 v_uv;

void main()
{
    v_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(v_uv * 2.0 - 1.0, 0.0, 1.0);
}
