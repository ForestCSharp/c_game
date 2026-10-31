#version 450
#extension GL_ARB_separate_shader_objects : enable

struct GuiVert
{
    vec2 position;
    vec2 uv;
    vec4 color;
    int draw_mode;
    uint glyph_index;
	uint padding[2];
};

layout(std430, set = 0, binding = 0) readonly buffer IndexBuffer {
	uint data[];
} index_buffer;

layout(std430, set = 0, binding = 1) readonly buffer VertexBuffer {
	GuiVert data[];
} vertex_buffer;


// Slug glyph data, must match TTFCurve (truetype.h) and GuiGlyph (gui.h)
struct TTFCurve
{
    vec2 p1;
    vec2 p2;
    vec2 p3;
};

struct GuiGlyph
{
    vec4 bounds;
    uint band_offset;
    uint band_count;
    uint padding[2];
};

layout(std430, set = 0, binding = 2) readonly buffer CurveBuffer {
	TTFCurve data[];
} curve_buffer;

layout(std430, set = 0, binding = 3) readonly buffer BandBuffer {
	uint data[];
} band_buffer;

layout(std430, set = 0, binding = 4) readonly buffer GlyphBuffer {
	GuiGlyph data[];
} glyph_buffer;

layout(location = 0) out vec2 out_position;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec4 out_color;
layout(location = 3) flat out int out_draw_mode;
layout(location = 4) flat out uint out_glyph_index;

/* Converts [0,1] to [-1,1] */
vec2 normalized_to_ndc(vec2 in_gui_coords)
{
    // flip Y first
    in_gui_coords.y = 1.0 - in_gui_coords.y;
    return (in_gui_coords * 2.0) - vec2(1, 1);
}

void main()
{
	uint indices_idx = gl_VertexIndex;
	uint vertices_index = index_buffer.data[indices_idx];
	GuiVert vertex = vertex_buffer.data[vertices_index];
	
    out_position = normalized_to_ndc(vertex.position);
    gl_Position = vec4(out_position, 0.0, 1.0);
    out_uv = vertex.uv;
    out_color = vertex.color;
	out_draw_mode = vertex.draw_mode;
	out_glyph_index = vertex.glyph_index;
}
