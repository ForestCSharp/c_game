#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Vertex 
{
	vec3 position;
};

layout(set = 0, binding = 0) readonly buffer VertexBuffer {
	Vertex data[];
} vertex_buffer;

layout(location = 0) out vec4 out_position;

void main()
{
	uint indices_idx = gl_VertexIndex;
	uint vertices_index = indices_idx;
	Vertex vertex = vertex_buffer.data[vertices_index];

	gl_Position = vec4(vertex.position, 1);
    out_position = gl_Position;
}
