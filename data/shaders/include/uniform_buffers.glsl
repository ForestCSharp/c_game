
#include "bindless.glsl"

layout(set = 1, binding = 0) uniform GlobalUniformBuffer {
    mat4 model;
	mat4 view;
	mat4 projection;
    mat4 mvp;
    vec4 eye;
    vec4 light_dir;
	bool is_colliding;
} global_ubo;

layout(set = 2, binding = 0) uniform PerDrawUniformBuffer {
    mat4 model;
	uint vertex_buffer_idx;
	uint index_buffer_idx;
} object_ubo;
