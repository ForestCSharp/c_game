#version 450
#extension GL_ARB_separate_shader_objects : enable

struct GuiVert
{
    vec2 position;
    vec2 uv;
    vec4 color;
    int has_texture;
	uint padding[3];
};

layout(std430, set = 0, binding = 0) readonly buffer IndexBuffer {
	uint data[];
} index_buffer;

layout(std430, set = 0, binding = 1) readonly buffer VertexBuffer {
	GuiVert data[];
} vertex_buffer;

layout(set = 0, binding = 2) uniform texture2D gui_texture; 
layout(set = 0, binding = 3) uniform sampler gui_sampler; 

layout(location = 0) out vec2 out_position;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec4 out_color;
layout(location = 3) flat out int out_has_texture;

/* Converts [0,1] to [-1,1] */
vec2 normalized_to_ndc(vec2 in_gui_coords)
{
    // flip Y first
    in_gui_coords.y = 1.0 - in_gui_coords.y;
    return (in_gui_coords * 2.0) - vec2(1, 1);
}

void main()
{
	uint indices_idx = gl_VertexIndex;
	uint vertices_index = index_buffer.data[indices_idx];
	GuiVert vertex = vertex_buffer.data[vertices_index];
	
    out_position = normalized_to_ndc(vertex.position);
    gl_Position = vec4(out_position, 0.0, 1.0);
    out_uv = vertex.uv;
    out_color = vertex.color;
	out_has_texture = vertex.has_texture;
}
