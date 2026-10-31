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

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;
layout(location = 3) flat in int in_draw_mode;
layout(location = 4) flat in uint in_glyph_index;

layout(location = 0) out vec4 out_color;

// Must match GuiDrawMode in gui.h
#define DRAW_MODE_SOLID 0
#define DRAW_MODE_GLYPH 1

// Slug (Lengyel 2017), ported from ttf_slug_ray_coverage in truetype.h.
// Fractional winding number change from a ray cast along +x from origin.
float slug_ray_coverage(vec2 p1, vec2 p2, vec2 p3, vec2 origin, float pixels_per_unit)
{
    float x1 = p1.x - origin.x;
    float y1 = p1.y - origin.y;

    // Which control points lie above the ray picks one of 8 classes; 0x2E74 holds each class's 2-bit root code
    uint shift = (y1 > 0.0 ? 2u : 0u) + (p2.y > origin.y ? 4u : 0u) + (p3.y > origin.y ? 8u : 0u);
    uint code = (0x2E74u >> shift) & 3u;
    if (code == 0u)
    {
        return 0.0;
    }

    // Curve is a*t^2 - 2*b*t + (p1 - origin). a and b come from the untranslated points so straight lines keep a == 0.
    vec2 a = p1 - 2.0 * p2 + p3;
    vec2 b = p1 - p2;

    float t1;
    float t2;
    if (abs(a.y) < 1.0 / 65536.0)
    {
        // Linear case
        t1 = y1 / (2.0 * b.y);
        t2 = t1;
    }
    else
    {
        // Discriminant is clamped so a curve that misses the ray yields two equal roots that cancel
        float d = sqrt(max(b.y * b.y - a.y * y1, 0.0));
        t1 = (b.y - d) / a.y;
        t2 = (b.y + d) / a.y;
    }

    float coverage = 0.0;
    if ((code & 1u) != 0u)
    {
        coverage += clamp(((a.x * t1 - 2.0 * b.x) * t1 + x1) * pixels_per_unit + 0.5, 0.0, 1.0);
    }
    if ((code & 2u) != 0u)
    {
        coverage -= clamp(((a.x * t2 - 2.0 * b.x) * t2 + x1) * pixels_per_unit + 0.5, 0.0, 1.0);
    }
    return coverage;
}

// Signed coverage of a ray cast from sample_pos through the band containing it, ported from ttf_slug_band_coverage.
// Axis 0 uses horizontal bands and a +x ray; axis 1 swaps x and y for vertical bands and a +y ray.
float slug_band_coverage(GuiGlyph glyph, uint axis, vec2 sample_pos, float pixels_per_unit)
{
    vec2 origin = axis == 0u ? sample_pos : sample_pos.yx;
    float band_min = axis == 0u ? glyph.bounds.y : glyph.bounds.x;
    float band_max = axis == 0u ? glyph.bounds.w : glyph.bounds.z;

    // Samples outside the glyph use the edge bands; nothing there can cross their rays
    float band = floor((origin.y - band_min) / (band_max - band_min) * float(glyph.band_count));
    uint band_index = uint(clamp(band, 0.0, float(glyph.band_count - 1u)));
    uint header = glyph.band_offset + 2u * (axis * glyph.band_count + band_index);
    uint curve_count = band_buffer.data[header];
    uint curve_list = band_buffer.data[header + 1u];

    float coverage = 0.0;
    for (uint i = 0u; i < curve_count; ++i)
    {
        TTFCurve curve = curve_buffer.data[band_buffer.data[curve_list + i]];
        vec2 p1 = axis == 0u ? curve.p1 : curve.p1.yx;
        vec2 p2 = axis == 0u ? curve.p2 : curve.p2.yx;
        vec2 p3 = axis == 0u ? curve.p3 : curve.p3.yx;

        // Curves are sorted by descending max x: once one ends over half a pixel behind the sample, so do the rest
        if ((max(max(p1.x, p2.x), p3.x) - origin.x) * pixels_per_unit < -0.5)
        {
            break;
        }
        coverage += slug_ray_coverage(p1, p2, p3, origin, pixels_per_unit);
    }
    return coverage;
}

void main() {
    // Pixels per font unit along each axis; derivatives are taken before any branching
    vec2 pixels_per_unit = 1.0 / fwidth(in_uv);

    if (in_draw_mode == DRAW_MODE_GLYPH)
    {
        GuiGlyph glyph = glyph_buffer.data[in_glyph_index];
        float x_coverage = slug_band_coverage(glyph, 0u, in_uv, pixels_per_unit.x);
        // The +y ray runs on swapped axes, which mirrors the outline and flips the winding sign; abs() absorbs it
        float y_coverage = slug_band_coverage(glyph, 1u, in_uv, pixels_per_unit.y);
        float coverage = (clamp(abs(x_coverage), 0.0, 1.0) + clamp(abs(y_coverage), 0.0, 1.0)) * 0.5;
        out_color = vec4(in_color.rgb, in_color.a * coverage);
    }
	else
	{
        out_color = in_color;
    }
}
