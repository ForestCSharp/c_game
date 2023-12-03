#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../include/uniform_buffers.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

void main() {

    out_color = vec4(0.0);

    //Diffuse
    vec3 normal = normalize(in_normal);
    vec3 light_dir = normalize(global_ubo.light_dir.xyz);
    float n_dot_l = max(dot(normal, -light_dir), 0.0);
    out_color += vec4(0.8,0.8,0.8,1) * n_dot_l;

    //Specular
    vec3 view_dir = normalize(global_ubo.eye.xyz - in_position);
    vec3 reflect_dir = reflect(light_dir, normal);
    float specular_strength = 1.0;
    vec4 specular_color = vec4(1,1,1,1);
    vec4 specular = specular_strength * pow(max(dot(view_dir, reflect_dir), 0.0), 32) * specular_color;
    out_color += specular;
    
}
