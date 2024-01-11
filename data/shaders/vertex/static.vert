#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../include/uniform_buffers.glsl"
#include "../include/static_vertex.glsl"

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec2 out_uv;

void main()
{
	StaticVertex vertex = load_static_vertex();
	gl_Position = global_ubo.projection * global_ubo.view * object_ubo.model * vertex.position;
    out_position = gl_Position.xyz;
    out_normal = mat3(transpose(inverse(object_ubo.model))) * vertex.normal.xyz;
    out_color = vertex.color;
    out_uv = vertex.uv;
}
