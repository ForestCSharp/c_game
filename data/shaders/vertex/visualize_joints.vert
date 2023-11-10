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
} joint_transforms;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec2 out_uv;

mat4 mat4_uniform_scale(float scale)
{
	return mat4(scale, 0, 0, 0,
				0, scale, 0, 0,
				0, 0, scale, 0,
				0, 0, 0, 1);
}

void main() {
	mat4 joint_vis_scale = mat4_uniform_scale(0.025);
    gl_Position = ubo.mvp * joint_transforms.data[gl_InstanceIndex] * joint_vis_scale * vec4(in_position, 1.0);

    out_position = gl_Position.xyz;

    out_normal = mat3(transpose(inverse(ubo.model))) * in_normal;

    out_color = in_color;

    out_uv = in_uv;
}
