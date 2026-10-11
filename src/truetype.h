#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include "basic_types.h"
#include "memory/allocator.h"

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

u16 ttf_find_glyph_index(const TTFFont* font, u32 codepoint);
u32 ttf_find_glyph_offset(const TTFFont* font, u16 glyph_index);
TTFGlyph ttf_find_glyph(const TTFFont* font, u32 codepoint);

i32 cmap_subtable_score(const CMapSubtable* subtable);

bool ttf_init(TTFFont* out_font, const char* in_filename)
{
    size_t file_size = 0;
    u8* file_data = NULL;
    if (!read_binary_file(in_filename, &file_size, (void**) &file_data))
    {
        return false;
    }

    *out_font = (TTFFont) {0};

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

            const i32 new_score = cmap_subtable_score(&out_font->cmap_subtables[i]);
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

    //FCS TODO: TESTING
    TTFGlyph glyph = ttf_find_glyph(out_font, 'a');

    return true;
}

TTFGlyph ttf_find_glyph(const TTFFont* font, u32 codepoint)
{
    u16 glyph_index = ttf_find_glyph_index(font, codepoint);
    assert(glyph_index != 0);
    printf("Found Glyph Index: %u\n", glyph_index);
    u32 glyph_offset = ttf_find_glyph_offset(font, glyph_index);
    printf("Found Glyph Offset: %u\n", glyph_offset);

    TTFGlyph glyph = {
        .num_contours   = be_i16(font->glyf + glyph_offset + 0),
        .x_min          = be_i16(font->glyf + glyph_offset + 2),
        .y_min          = be_i16(font->glyf + glyph_offset + 4),
        .x_max          = be_i16(font->glyf + glyph_offset + 6), 
        .y_max          = be_i16(font->glyf + glyph_offset + 8),
    };

    printf("glyph header: num_contours: %i x_min: %i y_min: %i x_max: %i y_max: %i\n",
            glyph.num_contours, glyph.x_min, glyph.y_min, glyph.x_max, glyph.y_max);

    if (glyph.num_contours >= 0)
    {
        // Simple Glyph
        printf("Simple Glyph\n");

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
        printf("Num Points: %u\n", num_points);

        u8 flags[num_points];
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
                    flags[points_filled++] = f; 
                }
            }
            else
            {
                flags[points_filled++] = f;
            }
        }

        // TODO: xCoordinates[variable] u8 or i16

        // TODO: yCoordinates[variable] u8 or i16
    }
    else
    {
        // Composite Glyph
        printf("Composite Glyph\n");
        //TODO:
    }


    return glyph;
}

i32 cmap_subtable_score(const CMapSubtable* subtable)
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

u16 ttf_find_glyph_index_format4(const u8* subtable_data, u32 codepoint)
{
    printf("\nFormat 4\n");
    printf("UNIMPLEMENTED!\n");
    assert(false);
}

u16 ttf_find_glyph_index_format12(const u8* subtable_data, u32 codepoint)
{
    printf("\nFormat 12\n");

    static const u32 SEQUENTIAL_MAP_GROUP_START = 16;
    static const u32 SEQUENTIAL_MAP_GROUP_SIZE = sizeof(u32) * 3;

    u32 length = be_u32(subtable_data + 4);
    u32 language = be_u32(subtable_data + 8);
    u32 num_groups = be_u32(subtable_data + 12);
    const u8 * groups = subtable_data + SEQUENTIAL_MAP_GROUP_START;

    printf("Length: %u Language: %u Num Groups: %u\n", length, language, num_groups);

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

    return -1;
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

void ttf_free(TTFFont* font)
{
    FCS_MEM_FREE((u8*)font->data);
}

float ttf_scale_for_pixel_height(const TTFFont* font, float height)
{
   return (float) height / (font->ascender - font->descender);
}

