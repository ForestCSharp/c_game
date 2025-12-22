#version 450
#extension GL_ARB_separate_shader_objects : enable

struct DebugDrawVertex
{
	vec4 position;
	vec4 normal;
	vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUniformBuffer {
	mat4 view;
	mat4 projection;
} global_ubo;

layout(set = 0, binding = 1) readonly buffer VertexBuffer {
	DebugDrawVertex data[];
} vertex_buffer;

layout(set = 0, binding = 2) readonly buffer IndexBuffer {
	uint data[];
} index_buffer;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec4 out_color;

void main()
{
	int indices_idx = gl_VertexIndex;
	uint vertices_index = index_buffer.data[indices_idx];
	DebugDrawVertex vertex = vertex_buffer.data[vertices_index];

	gl_Position = global_ubo.projection * global_ubo.view * vertex.position;
    out_position = gl_Position.xyz;
	const vec4 transformed_normal = global_ubo.view * vertex.normal;
	out_normal = transformed_normal.xyz;
	out_color = vertex.color;

}
