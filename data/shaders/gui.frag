#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec4 out_color;

void main() {
    if (in_uv.x >= 0.0f) {
         out_color = texture(tex_sampler, in_uv) * in_color;
    } else {
        out_color = in_color;
    }
}