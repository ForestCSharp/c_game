#version 450
#extension GL_ARB_separate_shader_objects : enable


struct StaticVertex
{
	vec4 position;
	vec4 normal;
	vec4 color;
	vec2 uv;
	uint padding[2];
};

layout(set = 0, binding = 0) readonly buffer VertexBuffer {
	StaticVertex data[];
} vertex_buffer;

layout(set = 0, binding = 1) readonly buffer IndexBuffer {
	uint data[];
} index_buffer;

layout(set = 0, binding = 2) readonly buffer UniformBuffer {
	mat4 model;
	mat4 view;
	mat4 projection;
} uniform_buffer;

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_color;

void main()
{
	uint indices_idx = gl_VertexIndex;
	uint vertices_index = index_buffer.data[indices_idx];
	StaticVertex vertex = vertex_buffer.data[vertices_index];

	gl_Position = uniform_buffer.projection * uniform_buffer.view * uniform_buffer.model * vertex.position;
    out_position = gl_Position;
	out_color = vertex.color;
}
