#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;
layout(location = 3) in int  in_has_texture;

layout(location = 0) out vec2 out_position;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec4 out_color;
layout(location = 3) flat out int out_has_texture;

/* Converts [0,1] to [-1,1] */
vec2 normalized_to_ndc(vec2 in_gui_coords)
{
    return (in_gui_coords * 2.0) - vec2(1,1);
}

void main() {
    out_position = normalized_to_ndc(in_position);
    gl_Position = vec4(out_position, 0.0, 1.0);
    out_uv = in_uv;
    out_color = in_color;
	out_has_texture = in_has_texture;
}
