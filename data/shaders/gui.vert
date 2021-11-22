#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 out_position;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec4 out_color;

void main() {
    out_position = in_position;
    out_color = in_color;
    out_uv = in_uv;
}