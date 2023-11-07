
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
	mat4 view;
	mat4 projection;
    mat4 mvp;
    vec4 eye;
    vec4 light_dir;	
} ubo;

layout(binding = 2) readonly buffer JointBuffer {
	mat4 data[];
} joint_buffer;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec4 in_joint_indices;
layout(location = 5) in vec4 in_joint_weights;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec2 out_uv;

void main() {

	//TODO: snake_case...
	mat4 skin_matrix = mat4(0.0);
	
	for (int i=0; i<4; ++i)
    {
        skin_matrix += (joint_buffer.data[int(in_joint_indices[i])] * in_joint_weights[i]);
    }

    //Skin matrix is identity if joint weights are all zero
    if ((abs(in_joint_weights[0] - 0.0)) < 0.000001)
    {
        skin_matrix = mat4(1.0);
    }

    gl_Position = ubo.mvp  * skin_matrix * vec4(in_position, 1.0);

    out_position = gl_Position.xyz;

    out_normal = mat3(transpose(inverse(ubo.model))) * in_normal;

    out_color = in_color;

    out_uv = in_uv;
}
