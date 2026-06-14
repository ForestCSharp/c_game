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
static u16 be_u16(const u8 *p)
{
    return (u16)((u32)p[0]<<8 | p[1]);
}
static u32 be_u32(const u8 *p)
{
    return  (u32)p[0]<<24
        |   (u32)p[1]<<16
        |   (u32)p[2]<<8
        |   p[3];
}
static i16 be_i16(const u8 *p)
{ 
    return (i16)be_u16(p);
}

static i32 be_i32(const u8 *p)
{
    return (i32)be_u32(p);
}

// Opaque font handle
typedef struct TTFFont
{
    const u8 *data;
    size_t size;
    const u8 *head, *hhea, *maxp, *hmtx, *cmap, *loca, *glyf;
    u16 numGlyphs;
    u16 unitsPerEm;
    i16  indexToLocFormat;
    i16  ascender, descender, lineGap;
    u16 numHMetrics;
} TTFFont;

// Per-glyph render metadata
typedef struct {
    u32 codepoint;
    u16 atlas_x;
    u16 atlas_y;
    u16 width;
    u16 height;
    i16 x_bearing;
    i16 y_bearing;
    i16 advance;
} TTFGlyphInfo;

typedef struct TableLoc
{ 
    u32 offset;
    u32 length; 
} TableLoc;

static TableLoc find_table(const u8 *data, u16 ntables,
                           const char tag[4]) {
    const u8 *dir = data + 12;
    for (u16 i = 0; i < ntables; i++) {
        if (memcmp(dir + i*16, tag, 4) == 0)
            return (TableLoc){ be_u32(dir + i*16 + 8), be_u32(dir + i*16 + 12) };
    }
    return (TableLoc){0, 0};
}

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

    out_font->unitsPerEm       = be_u16(out_font->head + 18);
    out_font->indexToLocFormat = be_i16(out_font->head + 50);
    out_font->numGlyphs        = be_u16(out_font->maxp + 4);
    out_font->ascender         = be_i16(out_font->hhea + 4);
    out_font->descender        = be_i16(out_font->hhea + 6);
    out_font->lineGap          = be_i16(out_font->hhea + 8);
    out_font->numHMetrics      = be_u16(out_font->hhea + 34);

    //FCS TODO: REMOVE
    //exit(0);

    return true;
}

void ttf_free(TTFFont *font)
{

}

float ttf_scale_for_pixel_height(const TTFFont *font, float px)
{

}

void ttf_get_vmetrics(const TTFFont *font, float scale, float *ascender, float *descender, float *line_gap)
{

}

