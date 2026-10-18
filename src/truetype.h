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
   const u8*    contours;
} TTFGlyph;

const i32 TTF_GLYPH_SIZE = 64;
const i32 TTF_ATLAS_SPAN = 1024;
const i32 TTF_ATLAS_GLYPHS_PER_ROW = TTF_ATLAS_SPAN / TTF_GLYPH_SIZE;
const i32 TTF_ATLAS_SIZE = TTF_ATLAS_SPAN * TTF_ATLAS_SPAN;

typedef struct TTFGlyphAtlasEntry {
    i32 x_idx;
    i32 y_idx;
    bool is_rasterized;
} TTFGlyphAtlasEntry;

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

typedef struct TTFEdgeFloat {
    TTFPointFloat start; 
    TTFPointFloat end;
} TTFEdgeFloat;

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

    u8* atlas_data;
    i32 next_glyph_atlas_idx;

    TTFGlyphAtlasEntry glyph_atlas_entries[128];
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
u8* ttf_get_glyph_atlas_data(const TTFFont* font, u32 codepoint);
void ttf_raster_glyph(TTFFont* font, u32 codepoint);
void ttf_print_glyph(TTFFont* font, u32 codepoint);
void ttf_write_ppm_atlas(const TTFFont* font);

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

    out_font->atlas_data = arena_alloc(out_font->arena, TTF_ATLAS_SIZE);
    assert(out_font->atlas_data);

    printf(
        "Chosen Subtable: platform_id: %u encoding_id: %u offset: %u\n", 
        out_font->chosen_cmap_subtable->platform_id, 
        out_font->chosen_cmap_subtable->encoding_id, 
        out_font->chosen_cmap_subtable->offset
    );

    for (u32 char_code = 0; char_code < 128; ++char_code)
    {
        printf("char_code: %u\n", char_code);
        ttf_raster_glyph(out_font, char_code);
        ttf_print_glyph(out_font, char_code);
    }
    ttf_write_ppm_atlas(out_font);

    return true;
}

void ttf_free(TTFFont* font)
{
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

typedef struct EdgeGenState {
    sbuffer(TTFEdgeFloat) edges;  

    bool bezier_in_progress;   
    TTFPointFloat bezier_p0;
    TTFPointFloat bezier_p1;
} EdgeGenState;

void ttf_push_edge(EdgeGenState* state, TTFPointFloat start, TTFPointFloat end)
{
    TTFEdgeFloat new_edge = {
        .start = start,
        .end = end,
    };
    sb_push(state->edges, new_edge);
}

TTFPointFloat ttf_midpoint(TTFPointFloat p0, TTFPointFloat p1)
{
   return (TTFPointFloat) {
        .x = (p0.x + p1.x) / 2.0f,
        .y = (p0.y + p1.y) / 2.0f,
   };
}

void ttf_emit_bezier(EdgeGenState* state, TTFPointFloat p0, TTFPointFloat p1, TTFPointFloat p2)
{
    const u32 segments = 8; 
    TTFPointFloat prev = p0;
    for (u32 i = 1; i <= segments; ++i)
    {
        const f32 t = (f32) i / (f32) segments;
        const f32 u = 1.0f - t;
        const f32 u_sq= u * u;
        const f32 t_sq = t * t;
        const f32 ut2 = u * t * 2;
        const TTFPointFloat pt = {
            .x = u_sq * p0.x + ut2 * p1.x + t_sq * p2.x,
            .y = u_sq * p0.y + ut2 * p1.y + t_sq * p2.y,
        };
        ttf_push_edge(state, prev, pt);
        prev = pt;
    }
}

u8* ttf_get_glyph_atlas_data(const TTFFont* font, u32 codepoint)
{
    const TTFGlyphAtlasEntry* entry = &font->glyph_atlas_entries[codepoint];
    if (!entry->is_rasterized) { return NULL; }

    u8* offset_atlas_data = font->atlas_data
        + (entry->y_idx * TTF_GLYPH_SIZE) * TTF_ATLAS_SPAN
        + (entry->x_idx * TTF_GLYPH_SIZE);
    return offset_atlas_data;
}

void ttf_raster_glyph(TTFFont* font, u32 codepoint)
{
    u16 glyph_index = ttf_find_glyph_index(font, codepoint);
    if (glyph_index == 0)
    {
        return;
    }

    u32 glyph_offset = ttf_find_glyph_offset(font, glyph_index);

    TTFGlyph glyph = {
        .num_contours   = be_i16(font->glyf + glyph_offset + 0),
        .x_min          = be_i16(font->glyf + glyph_offset + 2),
        .y_min          = be_i16(font->glyf + glyph_offset + 4),
        .x_max          = be_i16(font->glyf + glyph_offset + 6), 
        .y_max          = be_i16(font->glyf + glyph_offset + 8),
    };

    if (glyph.num_contours >= 0)
    {
        // Simple Glyph
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

        u8 instructions[instruction_length];
        for (i32 i = 0; i < instruction_length; ++i)
        {
            instructions[i] = be_u8(font->glyf + current_offset);
            current_offset += 1;
        }

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

        const i32 glyph_atlas_idx = font->next_glyph_atlas_idx++;
        const i32 glyph_atlas_x_idx = glyph_atlas_idx % TTF_ATLAS_GLYPHS_PER_ROW;
        const i32 glyph_atlas_y_idx = glyph_atlas_idx / TTF_ATLAS_GLYPHS_PER_ROW;
        printf("glyph atlas idx: %i x: %i y: %i\n", glyph_atlas_idx, glyph_atlas_x_idx, glyph_atlas_y_idx);

        font->glyph_atlas_entries[codepoint] = (TTFGlyphAtlasEntry) {
            .x_idx = glyph_atlas_x_idx,
            .y_idx = glyph_atlas_y_idx,
            .is_rasterized = true,
        }; 

        // Scale points based on pixel height (px)
        const f32 scale = ttf_scale_for_pixel_height(font, (f32) TTF_GLYPH_SIZE);
        TTFPointFloat points_float[num_points];
        for (u32 i = 0; i < num_points; ++i)
        {
            points_float[i].x = points[i].x * scale;
            points_float[i].y = points[i].y * scale;
            points_float[i].on_curve = points[i].on_curve;
        }

        EdgeGenState edge_gen_state = {
            .edges = NULL,
            .bezier_in_progress = false,
            .bezier_p0 = {},
            .bezier_p1 = {}
        };

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
                    ttf_push_edge(&edge_gen_state, current_point, next_point);
                }
                else if (current_point.on_curve && !next_point.on_curve)
                {
                    edge_gen_state.bezier_in_progress = true;
                    edge_gen_state.bezier_p0 = current_point;
                    edge_gen_state.bezier_p1 = next_point;
                }
                else if (!current_point.on_curve && !next_point.on_curve)
                {
                    TTFPointFloat implied = ttf_midpoint(current_point, next_point);
                    ttf_emit_bezier(&edge_gen_state, edge_gen_state.bezier_p0, edge_gen_state.bezier_p1, implied);
                    edge_gen_state.bezier_p0 = implied;
                    edge_gen_state.bezier_p1 = next_point; 
                    // bezier still in progress 
                }
                else if (!current_point.on_curve && next_point.on_curve)
                {
                    ttf_emit_bezier(&edge_gen_state, edge_gen_state.bezier_p0, edge_gen_state.bezier_p1, next_point);
                    edge_gen_state.bezier_in_progress = false;
                }
            } 
        }

        const f32 scaled_x_min = (f32) glyph.x_min * scale;
        const f32 scaled_y_min = (f32) glyph.y_min * scale;
        const f32 scaled_x_max = (f32) glyph.x_max * scale;
        const f32 scaled_y_max = (f32) glyph.y_max * scale;

        const u32 image_w = (u32)(scaled_x_max - scaled_x_min + 0.5f);
        const u32 image_h = (u32)(scaled_y_max - scaled_y_min + 0.5f);

        u8* offset_atlas_data = ttf_get_glyph_atlas_data(font, codepoint);
        assert(offset_atlas_data != NULL);

        for (u32 y = 0; y < TTF_GLYPH_SIZE; ++y)
        {
            memset(offset_atlas_data + y * TTF_ATLAS_SPAN, 0, TTF_GLYPH_SIZE);
        }

        const u32 edge_count = sb_count(edge_gen_state.edges);
        for (u32 i = 0; i < edge_count; ++i)
        {
            TTFEdgeFloat* edge = &edge_gen_state.edges[i];
            edge->start.x = edge->start.x - scaled_x_min;
            edge->start.y = scaled_y_max - edge->start.y; 
            edge->end.x = edge->end.x - scaled_x_min;
            edge->end.y = scaled_y_max - edge->end.y;
        }

        // Scanline Raster
        for (u32 y = 0; y < TTF_GLYPH_SIZE; ++y)
        {
            // Sample pixel y-center
            const f32 scan_y = (f32)y + 0.5f;

            // Crossings for this row
            f32 crossings[64];
            u32 crossings_count = 0;

            // Find Edge crossings for this row
            for (u32 e = 0; e < edge_count; ++e)
            { 
                // Get current edge and early-out if edge isn't on current row
                TTFEdgeFloat* edge = &edge_gen_state.edges[e];
                const f32 y0 = edge->start.y;
                const f32 y1 = edge->end.y;
                const f32 y_lo = (y0 < y1) ? y0 : y1;
                const f32 y_hi = (y0 < y1) ? y1 : y0;
                if (scan_y < y_lo || scan_y >= y_hi) { continue; }
                // Determine how far (t) we are on the line using our y coords
                const f32 t = (scan_y - y0) / (y1 - y0); 
                // Use that t-value to determine where x sits on this row
                const f32 x0 = edge->start.x; 
                const f32 x1 = edge->end.x;
                const f32 x = x0 + t * (x1 - x0);
                // That x value is our crossing
                crossings[crossings_count++] = x;
            }

            // insertion-sort crossings in ascending x-value
            for (u32 i = 1; i < crossings_count; ++i)
            {
                f32 v = crossings[i]; 
                u32 j = i; 
                while (j > 0 && crossings[j-1] > v)
                {
                    crossings[j] = crossings[j-1];
                    --j;
                }
                crossings[j] = v;
            }

            // Fill coverage image data via even-odd rule by counting by 2
            for (u32 i = 0; i < crossings_count; i += 2)
            {
                const f32 x_start = crossings[i];
                const f32 x_end  = crossings[i+1];

                // 0.5f so we sit at center of f32 pixel
                const u32 px_lo = (u32)(crossings[i] + 0.5f);
                const u32 px_hi = (u32)(crossings[i+1] + 0.5f);
                for (u32 x = px_lo; x < px_hi; ++x)
                {
                    if (x < 0 || x >= TTF_GLYPH_SIZE)
                    {
                        continue;
                    }

                    const f32 left = (f32)x; 
                    const f32 right = (f32) x + 1.0f;
                    const f32 cov_left = (x_start > left) ? x_start : left;
                    const f32 cov_right = (x_end < right) ? x_end : right;
                    f32 frac = cov_right - cov_left;
                    if (frac < 0.f) { frac = 0.f; }
                    offset_atlas_data[y * TTF_ATLAS_SPAN + x] = (u8) (frac * 255.f);
                }
            }
        }

        // Clean up edges
        sb_free(edge_gen_state.edges);
    }
    else
    {
        // Composite Glyph
        printf("Composite Glyph\n");
        printf("UNIMPLEMENTED!\n");
        exit(0);
        //TODO:
    }

}

void ttf_print_glyph(TTFFont* font, u32 codepoint)
{
    u8* offset_atlas_data = ttf_get_glyph_atlas_data(font, codepoint);
    if (offset_atlas_data == NULL) { return; }

    printf("\n");
    for (u32 y = 0; y < TTF_GLYPH_SIZE; y += 2)
    {
        for (u32 x = 0; x < TTF_GLYPH_SIZE; ++x)
        {
            const u8 top = offset_atlas_data[y * TTF_ATLAS_SPAN + x]; 
            const u8 bot = (y + 1 < TTF_GLYPH_SIZE) ? offset_atlas_data[((y + 1) * TTF_ATLAS_SPAN) + x] : 0;

            if (top == 0 && bot == 0)
            {
                printf(" ");
            }
            else
            {
                // ▀ renders top half in foreground, bottom half in background.                                                                                                                    
               // Set foreground to top pixel's gray, background to bottom pixel's gray.                                                                                                          
                printf(
                    "\x1b[38;2;%u;%u;%u;48;2;%u;%u;%um▀\x1b[0m",                                                                                                                                
                    top, top, top,  // foreground = top pixel gray                                                                                                                          
                    bot, bot, bot   // background = bottom pixel gray  
                );    
            }
        }
        printf("\n");
    }

}

void ttf_write_ppm_atlas(const TTFFont* font)
{
    FILE* f = fopen("bin/atlas.ppm", "wb");
    if (!f) { return; }

    fprintf(f, "P6\n%u %u\n255\n", TTF_ATLAS_SPAN, TTF_ATLAS_SPAN);

    const u32 pixel_count = (u32) TTF_ATLAS_SIZE;
    u8* rgb = FCS_MEM_ALLOC(pixel_count * 3);

    for (u32 i = 0; i < pixel_count; ++i)
    {
        const u8 grey = font->atlas_data[i]; 
        rgb[i * 3 + 0] = grey;
        rgb[i * 3 + 1] = grey;
        rgb[i * 3 + 2] = grey;
    }

    fwrite(rgb, 1, pixel_count * 3, f);
    FCS_MEM_FREE(rgb);
    fclose(f);
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

