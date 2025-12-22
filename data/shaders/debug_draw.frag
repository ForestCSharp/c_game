#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec4 out_color;

void main()
{
	//FCS TODO: 
	//out_color = in_color;

    // 1. Define a fixed light direction (top-down diagonal is best for volume)
    vec3 light_dir = normalize(vec3(0.3, 1.0, 0.3));
    
    float n_dot_l = dot(normalize(in_normal), light_dir);
    
    // 3. Map -1.0 -> 1.0 range to 0.0 -> 1.0 range
    // This ensures there is no "pure black" area
    float it = n_dot_l * 0.5 + 0.5;
    
    // Define Cool and Warm tones
    // We mix a bit of the vertex color (in_color) into both to keep the 'identity' 
    // of the debug object (e.g., a green collision box should still look green)
    vec3 cool = vec3(0.0, 0.0, 0.2) + 0.3 * in_color.rgb;
    vec3 warm = vec3(0.3, 0.3, 0.0) + 0.6 * in_color.rgb;
    
    vec3 rgb_result = mix(cool, warm, it);
    
    out_color = vec4(rgb_result, in_color.a);
}
