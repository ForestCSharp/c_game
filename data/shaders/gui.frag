#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec4 out_color;

#define TEXTURE_MULTISAMPLE 1

void main() {
    if (in_uv.x >= 0.0f) {
        out_color = vec4(0.0);

#if TEXTURE_MULTISAMPLE
        int window_size = 2;
        int total_samples = 0;
        for (int x = -window_size; x <= window_size; ++x) {
            for (int y = -window_size; y <= window_size; ++y) {
                total_samples++;
                const float sampling_offset = 1.0 / 1024.0; //TODO: compute this via texture size
                vec2 sample_uv = in_uv + (vec2(x,y) * sampling_offset);
                out_color += texture(tex_sampler, sample_uv) * in_color;
            }
        }
        out_color /= float(total_samples) * 0.5f;
#else
        out_color = texture(tex_sampler, in_uv) * in_color;
#endif
    } else {
        out_color = in_color;
    }
}