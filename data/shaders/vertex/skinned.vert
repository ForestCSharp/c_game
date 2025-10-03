
#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../include/uniform_buffers.glsl"
#include "../include/skinned_vertex.glsl"

layout(set = 2, binding = 1) readonly buffer JointBuffer {
	mat4 data[];
} joint_transforms;

layout(set = 2, binding = 2) buffer JointTransforms {
	mat4 data[];
} inverse_bind_matrices;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec2 out_uv;

void main()
{
	SkinnedVertex vertex = load_skinned_vertex();

	mat4 skin_matrix = mat4(0.0);
	
	for (int i=0; i<4; ++i)
    {
		int joint_idx = int(vertex.joint_indices[i]);
        skin_matrix += (joint_transforms.data[joint_idx] * inverse_bind_matrices.data[joint_idx] * vertex.joint_weights[i]);
    }

    //Skin matrix is identity if joint weights are all zero
    if ((abs(vertex.joint_weights[0] - 0.0)) < 0.000001)
    {
        skin_matrix = mat4(1.0);
    }

    gl_Position = global_ubo.projection * global_ubo.view * object_ubo.model * skin_matrix * vertex.position;

    out_position = gl_Position.xyz;

    out_normal = mat3(transpose(inverse(object_ubo.model))) * vertex.normal.xyz;

    out_color = vertex.color;

    out_uv = vertex.uv;
}
