#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "basic_types.h"
#include "memory/allocator.h"
#include "memory/arena.h"
#include "stretchy_buffer.h"
#include "math/vec.h"

// Byte-Order Helpers
static u8 be_u8(const u8* p)
{
    return p[0];
}

static u16 be_u16(const u8* p)
{
    return (u16)((u32)p[0]<<8 | p[1]);
}

static u32 be_u32(const u8* p)
{
    return  (u32)p[0]<<24
        |   (u32)p[1]<<16
        |   (u32)p[2]<<8
        |   p[3];
}

static i16 be_i16(const u8* p)
{ 
    return (i16)be_u16(p);
}

static i32 be_i32(const u8* p)
{
    return (i32)be_u32(p);
}

typedef struct TableLoc
{ 
    u32 offset;
    u32 length; 
} TableLoc;

typedef struct CMapSubtable {
    u16 platform_id;
    u16 encoding_id;
    u32 offset;
    const u8* data;
} CMapSubtable;

#define MAX_CMAP_SUBTABLES 16
#define CMAP_RECORD_SIZE 8   

typedef struct TTFGlyph {
   i16          num_contours;
   i16          x_min;
   i16          y_min;
   i16          x_max;
   i16          y_max;
} TTFGlyph;

typedef struct TTFPoint {
   i16 x;
   i16 y;
   bool on_curve;
} TTFPoint;

typedef struct TTFPointFloat {
    f32 x;
    f32 y;
    bool on_curve;
} TTFPointFloat;

// Quadratic Bezier in font units. Straight segments have p2 at the midpoint of p1 and p3.
typedef struct TTFCurve {
    Vec2 p1;
    Vec2 p2;
    Vec2 p3;
} TTFCurve;

typedef struct TTFGlyphOutline {
    i16 x_min;
    i16 y_min;
    i16 x_max;
    i16 y_max;
    u32 curve_offset; // Index of first curve in TTFFont.curves
    u32 curve_count;
    u32 band_offset;  // Index of first band header in TTFFont.band_data
    u32 band_count;   // Bands per direction
} TTFGlyphOutline;

#define TTF_MAX_BANDS 16

typedef enum TTFSimpleGlyphFlagBits {
    TTF_SIMPLE_GLYPH_FLAG_NONE                                  = 0,
    TTF_SIMPLE_GLYPH_FLAG_ON_CURVE_POINT                        = 1 << 0,
    TTF_SIMPLE_GLYPH_FLAG_X_SHORT_VECTOR                        = 1 << 1,
    TTF_SIMPLE_GLYPH_FLAG_Y_SHORT_VECTOR                        = 1 << 2,
    TTF_SIMPLE_GLYPH_FLAG_REPEAT_FLAG                           = 1 << 3,
    TTF_SIMPLE_GLYPH_FLAG_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR  = 1 << 4,
    TTF_SIMPLE_GLYPH_FLAG_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR  = 1 << 5,
    TTF_SIMPLE_GLYPH_FLAG_OVERLAP_SIMPLE                        = 1 << 6,
} TTFSimpleGlyphFlagBits; 

typedef struct TTFFont
{
    Arena* arena;

    const u8* data;
    size_t size;
    const u8* head;
    const u8* hhea;
    const u8* maxp;
    const u8* hmtx;
    const u8* cmap;
    const u8* loca;
    const u8* glyf;

    u16 num_glyphs;
    u16 units_per_em;
    i16 index_to_loc_format;
    i16 ascender;
    i16 descender;
    i16 line_gap;
    u16 num_h_metrics;

    u16 cmap_num_tables;
    CMapSubtable cmap_subtables[MAX_CMAP_SUBTABLES];
    CMapSubtable* chosen_cmap_subtable;

    sbuffer(TTFCurve) curves;
    TTFGlyphOutline glyph_outlines[128];

    // Per glyph: band_count horizontal then band_count vertical headers of {curve count, index of curve list},
    // followed by the curve lists (indices into curves)
    sbuffer(u32) band_data;
} TTFFont;

static TableLoc find_table(const u8 *data, u16 ntables,
                           const char tag[4]) {
    const u8 *dir = data + 12;
    for (u16 i = 0; i < ntables; i++) {
        if (memcmp(dir + i*16, tag, 4) == 0)
            return (TableLoc){ be_u32(dir + i*16 + 8), be_u32(dir + i*16 + 12) };
    }
    return (TableLoc){0, 0};
}

bool ttf_init(TTFFont* out_font, const char* in_filename);
void ttf_free(TTFFont* font);

u16 ttf_find_glyph_index(const TTFFont* font, u32 codepoint);
u32 ttf_find_glyph_offset(const TTFFont* font, u16 glyph_index);
void ttf_get_glyph_hmetric(const TTFFont* font, u32 codepoint, u16* out_advance, i16* out_lsb);
void ttf_build_curves(TTFFont* font, u32 codepoint);
void ttf_build_bands(TTFFont* font, u32 codepoint);
f32 ttf_slug_coverage(const TTFFont* font, u32 codepoint, f32 x, f32 y, f32 pixels_per_unit);

f32 ttf_scale_for_pixel_height(const TTFFont* font, f32 height);
i32 ttf_cmap_subtable_score(const CMapSubtable* subtable);

bool ttf_init(TTFFont* out_font, const char* in_filename)
{
    *out_font = (TTFFont) {0};

    out_font->arena = arena_create(&(ArenaDesc) {
        .size = 2 MiB,
        .allow_growth = true,
    });

    size_t file_size = 0;
    u8* file_data = NULL;
    if (!read_binary_file(in_filename, out_font->arena, &file_size, (void**) &file_data))
    {
        ttf_free(out_font);
        return false;
    }

    out_font->data = file_data;
    out_font->size = file_size;
    u16 ntables = be_u16(out_font->data + 4);

    #define FIND(tag, field) { \
        TableLoc t = find_table(out_font->data, ntables, tag); \
        out_font->field = out_font->data + t.offset; \
        printf("Found table '%s' at offset %u with length %u\n", tag, t.offset, t.length); \
    }
    FIND("head", head)
    FIND("hhea", hhea)
    FIND("maxp", maxp)
    FIND("hmtx", hmtx)
    FIND("cmap", cmap)
    FIND("loca", loca)
    FIND("glyf", glyf)
    #undef FIND

    out_font->units_per_em          = be_u16(out_font->head + 18);
    out_font->index_to_loc_format   = be_i16(out_font->head + 50);
    out_font->num_glyphs            = be_u16(out_font->maxp + 4);
    out_font->ascender              = be_i16(out_font->hhea + 4);
    out_font->descender             = be_i16(out_font->hhea + 6);
    out_font->line_gap              = be_i16(out_font->hhea + 8);
    out_font->num_h_metrics         = be_u16(out_font->hhea + 34);

    {   // CMAP 
        const u16 cmap_version = be_u16(out_font->cmap + 0);
        out_font->cmap_num_tables = be_u16(out_font->cmap + 2);
        assert(out_font->cmap_num_tables <= MAX_CMAP_SUBTABLES);

        printf("\n");
        printf("CMAP\n");
        printf("version: %u\n", cmap_version);
        printf("ntables: %u\n", out_font->cmap_num_tables);
        
        out_font->chosen_cmap_subtable = NULL;
        i32 current_chosen_score = -1;

        for (i32 i = 0; i < out_font->cmap_num_tables; ++i)
        {
            const u32 subtable_start = 4 + (i * CMAP_RECORD_SIZE);

            CMapSubtable subtable = {
                .platform_id = be_u16(out_font->cmap + subtable_start + 0),
                .encoding_id = be_u16(out_font->cmap + subtable_start + 2),
                .offset      = be_u32(out_font->cmap + subtable_start + 4),
            };
            subtable.data = out_font->cmap + subtable.offset;

            printf("\tSubtable %i platform_id: %u encoding_id: %u offset: %u\n",
                    i, subtable.platform_id, subtable.encoding_id, subtable.offset);

            out_font->cmap_subtables[i] = subtable;

            const i32 new_score = ttf_cmap_subtable_score(&out_font->cmap_subtables[i]);
            if (new_score > current_chosen_score)
            {
                out_font->chosen_cmap_subtable = &out_font->cmap_subtables[i];
                current_chosen_score = new_score;
            }
        }
    }

    printf(
        "Chosen Subtable: platform_id: %u encoding_id: %u offset: %u\n", 
        out_font->chosen_cmap_subtable->platform_id, 
        out_font->chosen_cmap_subtable->encoding_id, 
        out_font->chosen_cmap_subtable->offset
    );

    for (u32 char_code = 0; char_code < 128; ++char_code)
    {
        ttf_build_curves(out_font, char_code);
        ttf_build_bands(out_font, char_code);
    }

    return true;
}

void ttf_free(TTFFont* font)
{
    sb_free(font->curves);
    sb_free(font->band_data);
    arena_destroy(&font->arena);
    *font = (TTFFont) {0};
}

u16 ttf_find_glyph_index_format4(const u8* subtable_data, u32 codepoint)
{
    printf("\nFormat 4\n");
    printf("UNIMPLEMENTED!\n");
    assert(false);
}

u16 ttf_find_glyph_index_format12(const u8* subtable_data, u32 codepoint)
{
    static const u32 SEQUENTIAL_MAP_GROUP_START = 16;
    static const u32 SEQUENTIAL_MAP_GROUP_SIZE = sizeof(u32) * 3;

    u32 length = be_u32(subtable_data + 4);
    u32 language = be_u32(subtable_data + 8);
    u32 num_groups = be_u32(subtable_data + 12);
    const u8 * groups = subtable_data + SEQUENTIAL_MAP_GROUP_START;

    // Binary Search over groups to find correct one
    u32 low = 0;
    u32 high = num_groups;
    while (low < high)
    {
        const u32 mid = low + (high - low) / 2;
        const u8* group = groups + (mid * SEQUENTIAL_MAP_GROUP_SIZE);

        u32 start_char_code = be_u32(group + 0);
        u32 end_char_code   = be_u32(group + 4);

        if (codepoint < start_char_code)
        {
           high = mid; 
        }
        else if (codepoint > end_char_code)
        {
           low = mid + 1; 
        }
        else
        {
            u32 start_glyph_id  = be_u32(group + 8);
            return (u16) (start_glyph_id + (codepoint - start_char_code));
        }
    }

    return 0;
}

u16 ttf_find_glyph_index(const TTFFont* font, u32 codepoint)
{
    const u8* subtable_data = font->chosen_cmap_subtable->data;
    const u16 cmap_format = be_u16(subtable_data);
    switch (cmap_format)
    {
        case 4: return ttf_find_glyph_index_format4(subtable_data, codepoint);
        case 12: return ttf_find_glyph_index_format12(subtable_data, codepoint);
        default: printf("ERROR: Unsupported cmap format\n"); assert(false);
    }
}

u32 ttf_find_glyph_offset(const TTFFont* font, u16 glyph_index)
{
    u16 num_glyphs;
    u16 units_per_em;
    i16 index_to_loc_format;

    u32 offset = 0;
    switch(font->index_to_loc_format)
    {
        case 0: return 2 * (u32) be_u16(font->loca + (sizeof(u16) * glyph_index));
        case 1: return be_u32(font->loca + (sizeof(u32) * glyph_index));
        default: assert(false); return 0;
    } 
}

TTFPointFloat ttf_midpoint(TTFPointFloat p0, TTFPointFloat p1)
{
   return (TTFPointFloat) {
        .x = (p0.x + p1.x) / 2.0f,
        .y = (p0.y + p1.y) / 2.0f,
   };
}

void ttf_get_glyph_hmetric(const TTFFont* font, u32 codepoint, u16* out_advance, i16* out_lsb)
{
    u16 glyph_index = ttf_find_glyph_index(font, codepoint);
   if (glyph_index == 0) { return; }  // not mapped

   if (glyph_index < font->num_h_metrics)
   {
       // Long metric: {u16 advanceWidth, i16 lsb} at hmtx + glyph_index * 4
       const u8* m = font->hmtx + glyph_index * 4;
       *out_advance = be_u16(m + 0);
       *out_lsb = be_i16(m + 2);
   }
   else
   {
       // Short metric: share last advance, read lsb from the lsb array
       const u8* last = font->hmtx + (font->num_h_metrics - 1) * 4;
       *out_advance = be_u16(last + 0);
       *out_lsb = be_i16(font->hmtx + font->num_h_metrics * 4 + (glyph_index - font->num_h_metrics) * 2);
   }    
}

void ttf_push_curve(TTFFont* font, TTFPointFloat p1, TTFPointFloat p2, TTFPointFloat p3)
{
    TTFCurve curve = {
        .p1 = { .x = p1.x, .y = p1.y },
        .p2 = { .x = p2.x, .y = p2.y },
        .p3 = { .x = p3.x, .y = p3.y },
    };
    sb_push(font->curves, curve);
}

// Converts a glyph's outline into quadratic curves (in font units) appended to font->curves
void ttf_build_curves(TTFFont* font, u32 codepoint)
{
    u16 glyph_index = ttf_find_glyph_index(font, codepoint);
    if (glyph_index == 0)
    {
        return;
    }

    u32 glyph_offset = ttf_find_glyph_offset(font, glyph_index);
    u32 glyph_length = ttf_find_glyph_offset(font, glyph_index + 1) - glyph_offset;

    TTFGlyphOutline* outline = &font->glyph_outlines[codepoint];
    *outline = (TTFGlyphOutline) {
        .curve_offset = sb_count(font->curves),
        .curve_count = 0,
    };

    if (glyph_length == 0)
    {
        // Empty glyph (e.g. space): no curves
        return;
    }

    TTFGlyph glyph = {
        .num_contours   = be_i16(font->glyf + glyph_offset + 0),
        .x_min          = be_i16(font->glyf + glyph_offset + 2),
        .y_min          = be_i16(font->glyf + glyph_offset + 4),
        .x_max          = be_i16(font->glyf + glyph_offset + 6), 
        .y_max          = be_i16(font->glyf + glyph_offset + 8),
    };
    outline->x_min = glyph.x_min;
    outline->y_min = glyph.y_min;
    outline->x_max = glyph.x_max;
    outline->y_max = glyph.y_max;

    if (glyph.num_contours < 0)
    {
        // Composite Glyph
        printf("Composite Glyph\n");
        printf("UNIMPLEMENTED!\n");
        exit(0);
        //TODO:
    }

    u16 end_points_of_contours[glyph.num_contours];
    const u32 GLYPH_HEADER_SIZE = 10;
    u32 current_offset = glyph_offset + GLYPH_HEADER_SIZE;
    for (i32 i = 0; i < glyph.num_contours; ++i)
    {
        end_points_of_contours[i] = be_u16(font->glyf + current_offset);
        current_offset += 2;
    }
    u16 instruction_length = be_u16(font->glyf + current_offset);
    current_offset += 2;

    current_offset += instruction_length; // Skip hinting instructions

    const u32 num_points = end_points_of_contours[glyph.num_contours - 1] + 1;

    u8 flags[num_points];
    TTFPoint points[num_points];

    u32 points_filled = 0;
    while (points_filled < num_points)
    {
        u8 f = be_u8(font->glyf + current_offset);
        current_offset += 1;

        if (f & TTF_SIMPLE_GLYPH_FLAG_REPEAT_FLAG)
        {
            u8 count = be_u8(font->glyf + current_offset);
            current_offset += 1;

            for (u32 r = 0; r <= count && points_filled < num_points; ++r)
            {
                points[points_filled].on_curve = (f & TTF_SIMPLE_GLYPH_FLAG_ON_CURVE_POINT) != 0;
                flags[points_filled] = f; 
                points_filled++;
            }
        }
        else
        {
            points[points_filled].on_curve = (f & TTF_SIMPLE_GLYPH_FLAG_ON_CURVE_POINT) != 0;
            flags[points_filled] = f; 
            points_filled++;
        }
    }

    // Get point x-values, built up via an accumulator and deltas
    i32 x_accumulator = 0;
    for (u32 i = 0; i < num_points; ++i)
    {
        const u8 f = flags[i];
        i16 dx = 0;

        if (f & TTF_SIMPLE_GLYPH_FLAG_X_SHORT_VECTOR)
        {
            // We always read the unsigned short value
            const u8 value = be_u8(font->glyf + current_offset);
            current_offset += 1;

            // for short values, this flag determines if delta is positive or negative
            if (f & TTF_SIMPLE_GLYPH_FLAG_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR)
            {
               dx = (i16) value; 
            }
            else
            {
                dx = - (i16) value;
            }
        }
        else
        {
            // for long values, this flag determines if delta is same (0) or to read the new long
            if (f & TTF_SIMPLE_GLYPH_FLAG_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR)
            {
               dx = 0;
            }
            else
            {
                // read the signed long value
                dx = be_i16(font->glyf + current_offset);
                current_offset += 2;
            }

        }

        x_accumulator += dx;
        points[i].x = (i16)x_accumulator;
                    
    }

    // Get point y-values, built up via an accumulator and deltas
    i32 y_accumulator = 0;
    for (u32 i = 0; i < num_points; ++i)
    {
        const u8 f = flags[i];
        i16 dy = 0;

        if (f & TTF_SIMPLE_GLYPH_FLAG_Y_SHORT_VECTOR)
        {
            // We always read the unsigned short value
            const u8 value = be_u8(font->glyf + current_offset);
            current_offset += 1;

            // for short values, this flag determines if delta is positive or negative
            if (f & TTF_SIMPLE_GLYPH_FLAG_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR)
            {
               dy = (i16) value; 
            }
            else
            {
                dy = - (i16) value;
            }
        }
        else
        {
            // for long values, this flag determines if delta is same (0) or to read the new long
            if (f & TTF_SIMPLE_GLYPH_FLAG_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR)
            {
               dy = 0;
            }
            else
            {
                // Read the signed long value
                dy = be_i16(font->glyf + current_offset);
                current_offset += 2;
            }

        }

        y_accumulator += dy;
        points[i].y = (i16)y_accumulator;     
    }

    TTFPointFloat points_float[num_points];
    for (u32 i = 0; i < num_points; ++i)
    {
        points_float[i].x = points[i].x;
        points_float[i].y = points[i].y;
        points_float[i].on_curve = points[i].on_curve;
    }

    // Start and control point of the curve currently being built
    TTFPointFloat bezier_p0 = {};
    TTFPointFloat bezier_p1 = {};

    for (i16 c = 0; c < glyph.num_contours; ++c)
    {
        u16 start = c == 0 ? 0 : end_points_of_contours[c-1] + 1;
        u16 end = end_points_of_contours[c];
        const u16 n = (end - start) + 1;

        u16 rot = start;
        while (rot <= end && !points_float[rot].on_curve)
        {
            ++rot; 
        }
        assert(rot <= end);

        for (u16 step = 0; step < n; ++step)
        {
            const u16 current = start + ((rot - start) + step) % n;  
            const u16 next = start + ((rot - start) + step + 1) % n;  

            TTFPointFloat current_point = points_float[current];
            TTFPointFloat next_point = points_float[next];

            if (current_point.on_curve && next_point.on_curve)
            {
                ttf_push_curve(font, current_point, ttf_midpoint(current_point, next_point), next_point);
            }
            else if (current_point.on_curve && !next_point.on_curve)
            {
                bezier_p0 = current_point;
                bezier_p1 = next_point;
            }
            else if (!current_point.on_curve && !next_point.on_curve)
            {
                TTFPointFloat implied = ttf_midpoint(current_point, next_point);
                ttf_push_curve(font, bezier_p0, bezier_p1, implied);
                bezier_p0 = implied;
                bezier_p1 = next_point; 
            }
            else if (!current_point.on_curve && next_point.on_curve)
            {
                ttf_push_curve(font, bezier_p0, bezier_p1, next_point);
            }
        } 
    }

    outline->curve_count = sb_count(font->curves) - outline->curve_offset;
}

static f32 ttf_saturate(f32 value)
{
    return fminf(fmaxf(value, 0.0f), 1.0f);
}

// Slug (Lengyel 2017): fractional winding number change from a ray cast along +x from origin.
f32 ttf_slug_ray_coverage(Vec2 p1, Vec2 p2, Vec2 p3, Vec2 origin, f32 pixels_per_unit)
{
    const f32 x1 = p1.x - origin.x;
    const f32 y1 = p1.y - origin.y;

    // Which control points lie above the ray picks one of 8 classes; 0x2E74 holds each class's 2-bit root code
    const u32 shift = (y1 > 0.0f ? 2 : 0) + (p2.y > origin.y ? 4 : 0) + (p3.y > origin.y ? 8 : 0);
    const u32 code = (0x2E74u >> shift) & 3u;
    if (code == 0)
    {
        return 0.0f;
    }

    // Curve is a*t^2 - 2*b*t + (p1 - origin). a and b are translation invariant, so they're computed from the
    // untranslated points: translating first adds rounding that makes straight lines (a == 0) look curved.
    const f32 ax = p1.x - 2.0f * p2.x + p3.x;
    const f32 ay = p1.y - 2.0f * p2.y + p3.y;
    const f32 bx = p1.x - p2.x;
    const f32 by = p1.y - p2.y;

    f32 t1, t2;
    if (fabsf(ay) < 1.0f / 65536.0f)
    {
        // Linear case
        t1 = t2 = y1 / (2.0f * by);
    }
    else
    {
        // Discriminant is clamped so a curve that misses the ray yields two equal roots that cancel
        const f32 d = sqrtf(fmaxf(by * by - ay * y1, 0.0f));
        t1 = (by - d) / ay;
        t2 = (by + d) / ay;
    }

    f32 coverage = 0.0f;
    if (code & 1u)
    {
        coverage += ttf_saturate(((ax * t1 - 2.0f * bx) * t1 + x1) * pixels_per_unit + 0.5f);
    }
    if (code & 2u)
    {
        coverage -= ttf_saturate(((ax * t2 - 2.0f * bx) * t2 + x1) * pixels_per_unit + 0.5f);
    }
    return coverage;
}

static Vec2 ttf_swap_xy(Vec2 v)
{
    return vec2_new(v.y, v.x);
}

static f32 ttf_min3(f32 a, f32 b, f32 c)
{
    return fminf(a, fminf(b, c));
}

static f32 ttf_max3(f32 a, f32 b, f32 c)
{
    return fmaxf(a, fmaxf(b, c));
}

// Band axis 0 holds horizontal bands (rays along +x), axis 1 vertical bands (rays along +y). Axis 1 swaps x and y,
// so in both cases bands span y and rays run along x.
static TTFCurve ttf_curve_for_axis(const TTFCurve* curve, u32 axis)
{
    if (axis == 0)
    {
        return *curve;
    }
    return (TTFCurve) { ttf_swap_xy(curve->p1), ttf_swap_xy(curve->p2), ttf_swap_xy(curve->p3) };
}

// Splits a glyph into equal-width horizontal and vertical bands, each listing only the curves a ray inside it can
// cross, sorted by descending max coordinate along the ray so coverage can stop early
void ttf_build_bands(TTFFont* font, u32 codepoint)
{
    TTFGlyphOutline* outline = &font->glyph_outlines[codepoint];
    outline->band_offset = sb_count(font->band_data);
    outline->band_count = 0;
    if (outline->curve_count == 0)
    {
        return;
    }

    u32 band_count = outline->curve_count / 4;
    band_count = band_count < 1 ? 1 : band_count > TTF_MAX_BANDS ? TTF_MAX_BANDS : band_count;
    outline->band_count = band_count;
    sb_add(font->band_data, 2 * 2 * band_count); // Headers, filled in below

    for (u32 axis = 0; axis < 2; ++axis)
    {
        const f32 band_min = axis == 0 ? outline->y_min : outline->x_min;
        const f32 band_size = ((axis == 0 ? outline->y_max : outline->x_max) - band_min) / (f32) band_count;

        for (u32 band = 0; band < band_count; ++band)
        {
            // A font unit of slack so a sample that rounds into the neighbouring band still finds its curves
            const f32 band_lo = band_min + band * band_size - 1.0f;
            const f32 band_hi = band_min + (band + 1) * band_size + 1.0f;
            const u32 list_offset = sb_count(font->band_data);

            for (u32 i = 0; i < outline->curve_count; ++i)
            {
                const u32 curve_index = outline->curve_offset + i;
                const TTFCurve c = ttf_curve_for_axis(&font->curves[curve_index], axis);

                // Curves parallel to the ray never cross it
                const bool parallel = c.p1.y == c.p2.y && c.p2.y == c.p3.y;
                if (parallel || ttf_max3(c.p1.y, c.p2.y, c.p3.y) < band_lo || ttf_min3(c.p1.y, c.p2.y, c.p3.y) > band_hi)
                {
                    continue;
                }

                // Insertion sort, descending by max x
                const f32 key = ttf_max3(c.p1.x, c.p2.x, c.p3.x);
                sb_push(font->band_data, curve_index);
                u32 j = sb_count(font->band_data) - 1;
                while (j > list_offset)
                {
                    const TTFCurve prev = ttf_curve_for_axis(&font->curves[font->band_data[j - 1]], axis);
                    if (ttf_max3(prev.p1.x, prev.p2.x, prev.p3.x) >= key)
                    {
                        break;
                    }
                    font->band_data[j] = font->band_data[j - 1];
                    --j;
                }
                font->band_data[j] = curve_index;
            }

            u32* header = &font->band_data[outline->band_offset + 2 * (axis * band_count + band)];
            header[0] = sb_count(font->band_data) - list_offset;
            header[1] = list_offset;
        }
    }
}

// Signed coverage of a ray cast from sample through the band containing it (see ttf_curve_for_axis for axes)
static f32 ttf_slug_band_coverage(const TTFFont* font, const TTFGlyphOutline* outline, u32 axis, Vec2 sample, f32 pixels_per_unit)
{
    const Vec2 origin = axis == 0 ? sample : ttf_swap_xy(sample);
    const f32 band_min = axis == 0 ? outline->y_min : outline->x_min;
    const f32 band_max = axis == 0 ? outline->y_max : outline->x_max;

    // Samples outside the glyph use the edge bands; nothing there can cross their rays
    const f32 band = floorf((origin.y - band_min) / (band_max - band_min) * (f32) outline->band_count);
    const u32 band_index = (u32) fminf(fmaxf(band, 0.0f), (f32) (outline->band_count - 1));
    const u32* header = &font->band_data[outline->band_offset + 2 * (axis * outline->band_count + band_index)];

    f32 coverage = 0.0f;
    for (u32 i = 0; i < header[0]; ++i)
    {
        const TTFCurve c = ttf_curve_for_axis(&font->curves[font->band_data[header[1] + i]], axis);

        // Once a curve ends more than half a pixel behind the sample it adds nothing, and neither do the rest
        if ((ttf_max3(c.p1.x, c.p2.x, c.p3.x) - origin.x) * pixels_per_unit < -0.5f)
        {
            break;
        }
        coverage += ttf_slug_ray_coverage(c.p1, c.p2, c.p3, origin, pixels_per_unit);
    }
    return coverage;
}

// Coverage in [0,1] at (x, y) in font units, averaging a +x ray and a +y ray
f32 ttf_slug_coverage(const TTFFont* font, u32 codepoint, f32 x, f32 y, f32 pixels_per_unit)
{
    const TTFGlyphOutline* outline = &font->glyph_outlines[codepoint];
    if (outline->band_count == 0)
    {
        return 0.0f;
    }

    const Vec2 sample = vec2_new(x, y);
    const f32 x_coverage = ttf_slug_band_coverage(font, outline, 0, sample, pixels_per_unit);
    // The +y ray runs on swapped axes, which mirrors the outline and flips the winding sign; abs() below absorbs it
    const f32 y_coverage = ttf_slug_band_coverage(font, outline, 1, sample, pixels_per_unit);

    return (ttf_saturate(fabsf(x_coverage)) + ttf_saturate(fabsf(y_coverage))) * 0.5f;
}

f32 ttf_scale_for_pixel_height(const TTFFont* font, f32 height)
{
   return (f32) height / (font->ascender - font->descender);
}

i32 ttf_cmap_subtable_score(const CMapSubtable* subtable)
{
    // format 12, full Unicode — best
    if ((subtable->platform_id == 3 && subtable->encoding_id == 10) ||
        (subtable->platform_id == 0 && subtable->encoding_id ==  4) ||
        (subtable->platform_id == 0 && subtable->encoding_id ==  6))
    {
        return 100;
    }
    // format 4, BMP Unicode — good fallback
    if ((subtable->platform_id == 3 && subtable->encoding_id == 1) || 
        (subtable->platform_id == 0 && subtable->encoding_id == 3))
    {
        return 50;
    }

    return -1;   // unsupported (format 0/2/6/etc.)
}

