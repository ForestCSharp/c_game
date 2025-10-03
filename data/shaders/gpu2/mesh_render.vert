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

struct SkinnedVertex
{
    vec4 joint_indices;
    vec4 joint_weights;
};

// Set 0 is global
layout(std430, set = 0, binding = 0) readonly buffer GlobalUniformBuffer {
	mat4 view;
	mat4 projection;
	vec4 eye;
	vec4 light_dir;
} global_uniform_buffer;

// Set 1 is per draw data
layout(std430, set = 1, binding = 0) readonly buffer PerDrawUniformBuffer {
    mat4 model;
	bool is_skinned;
	uint padding[3];
} object_ubo;

layout(std430, set = 1, binding = 1) readonly buffer VertexBuffer {
	StaticVertex data[];
} vertex_buffer;

layout(std430, set = 1, binding = 2) readonly buffer IndexBuffer {
	uint data[];
} index_buffer;

// Resources Below only used by animated models
layout(std430, set = 1, binding = 3) readonly buffer SkinnedVertexBuffer {
	SkinnedVertex data[];
} skinned_vertex_buffer;

layout(std430, set = 1, binding = 4) buffer JointTransforms {
	mat4 data[];
} inverse_bind_matrices;

layout(std430, set = 1, binding = 5) readonly buffer JointBuffer {
	mat4 data[];
} joint_transforms;

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_color;

void main()
{
	uint indices_idx = gl_VertexIndex;
	uint vertices_index = index_buffer.data[indices_idx];
	StaticVertex vertex = vertex_buffer.data[vertices_index];

	mat4 skin_matrix = mat4(1.0);
	if (object_ubo.is_skinned)
	{
		SkinnedVertex skinned_vertex = skinned_vertex_buffer.data[vertices_index];

		//Skin matrix is identity if joint weights are all zero
		if ((abs(skinned_vertex.joint_weights[0] - 0.0)) >= 0.000001)
		{
			skin_matrix = mat4(0.0);
			for (int i=0; i<4; ++i)
			{
				int joint_idx = int(skinned_vertex.joint_indices[i]);
				skin_matrix += (joint_transforms.data[joint_idx] * inverse_bind_matrices.data[joint_idx] * skinned_vertex.joint_weights[i]);
			}
		}
	}

	gl_Position = global_uniform_buffer.projection * global_uniform_buffer.view * object_ubo.model * skin_matrix * vertex.position;
    out_position = gl_Position;
	out_color = vec4(vertex.normal.xyz, 1);
}
