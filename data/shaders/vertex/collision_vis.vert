#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
	mat4 view;
	mat4 projection;
    mat4 mvp;
    vec4 eye;
    vec4 light_dir;
	bool is_colliding;
} ubo;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec2 out_uv;

void main() {
    gl_Position = ubo.mvp * vec4(in_position, 1.0);

    out_position = gl_Position.xyz;

    out_normal = mat3(transpose(inverse(ubo.model))) * in_normal;

    out_color = ubo.is_colliding ? vec4(1,0,0,0) : vec4(0,0,0,0);

    out_uv = in_uv;
}
