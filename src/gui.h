#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "stretchy_buffer.h"
#include "vec.h"

typedef struct GuiVert {
    Vec2 position;
    Vec2 uv;
    Vec4 color;
} GuiVert;

typedef struct GuiFrameState {
    uint32_t res_x;
    uint32_t res_y;

    int32_t mouse_x;
    int32_t mouse_y;
    bool mouse_buttons[3];

    sbuffer(GuiVert) vertices;
    sbuffer(uint32_t) indices;
} GuiFrameState;

#define BFF_ID 0xF2BF

typedef enum GuiFontType {
    FONT_TYPE_ALPHA = 8,
    FONT_TYPE_RGB   = 24,
    FONT_TYPE_RGBA  = 32,
} GuiFontType;

typedef struct GuiFont {
    uint32_t            image_width;
    uint32_t            image_height;
    uint32_t            cell_width;
    uint32_t            cell_height;
    GuiFontType         font_type;
    char                first_char;
    uint8_t             char_widths[256];
    sbuffer(uint8_t)    image_data;
    uint64_t            image_data_size;
} GuiFont;

typedef struct GuiContext {
    GuiFrameState frame_state;
} GuiContext;

//TODO: take in a default font
void gui_init(GuiContext* out_context) {
    *out_context = (GuiContext){
        .frame_state = (GuiFrameState) {
            .res_x = 1920,
            .res_y = 1080,
            .mouse_x = 0,
            .mouse_y = 0,
            .mouse_buttons = {false, false, false},
            .vertices = NULL,
            .indices = NULL,
        },
    };
}

void gui_shutdown(GuiContext* in_context) {
    sb_free(in_context->frame_state.vertices);
    sb_free(in_context->frame_state.indices);
}

//Zeroes out out_font before loading, make sure out_font wasn't previously created with this function, and if so, free it with gui_free_font
bool gui_load_font(const char* filename, GuiFont* out_font) {
    FILE* file = fopen(filename, "rb"); 

    if (file && out_font) {
        *out_font = (GuiFont){};

        uint16_t bff_id;
        if (fread(&bff_id, 2, 1, file) != 1 || bff_id != BFF_ID) {
            return false;
        }
        
        if (fread(&out_font->image_width, 4, 1, file) != 1) {
            return false;
        }
        if (fread(&out_font->image_height, 4, 1, file) != 1) {
            return false;
        }
        if (fread(&out_font->cell_width, 4, 1, file) != 1) {
            return false;
        }
        if (fread(&out_font->cell_height, 4, 1, file) != 1) {
            return false;
        }

        uint8_t font_type;
        if (fread(&font_type, 1, 1, file) == 1) {
            out_font->font_type = font_type;
        } else {
            return false;
        }

        if (fread(&out_font->first_char, 1, 1, file) != 1) {
            return false;
        }

        if (fread(out_font->char_widths, 1, 256, file) != 256) {
            return false;
        }

        //Finally, load actual image data
        out_font->image_data_size = out_font->image_width * out_font->image_height * (font_type / 8);
        
        sb_add(out_font->image_data, out_font->image_data_size);      
        if (fread(out_font->image_data, 1, out_font->image_data_size, file) != out_font->image_data_size) {
            return false;
        }

        printf("Font Image Width: %i Height: %i\n", out_font->image_width, out_font->image_height);
        printf("Cell Width: %i Height: %i\n", out_font->cell_width, out_font->cell_height);
        printf("Font Type: %i First Char: %i\n", out_font->font_type, out_font->first_char);

        return true;
    }

    printf("Failed to load font\n");

    return false;
}

typedef struct CharUVs {
    float min_u;
    float min_v;
    float max_u;
    float max_v;
} CharUVs;

bool gui_font_get_uvs(GuiFont* in_font, char in_char, CharUVs* out_uvs) {
   
    int32_t chars_per_row = in_font->image_width / in_font->cell_width;
    int32_t chars_per_col = in_font->image_height / in_font->cell_height;
    int32_t total_chars = chars_per_row * chars_per_col;
    
    if (in_char >= in_font->first_char && in_char <= in_font->first_char + total_chars) {
        int32_t row = (in_char - in_font->first_char) / chars_per_row;
        int32_t col = (in_char - in_font->first_char) - (row * chars_per_row);

        float row_factor = (float)in_font->cell_height / (float)in_font->image_height;
        float col_factor = (float)in_font->cell_width  / (float)in_font->image_width;

        float min_u = col * col_factor;
        float min_v = row * row_factor;

        *out_uvs = (CharUVs) {
            .min_u = min_u,
            .min_v = min_v,
            .max_u = min_u + col_factor,
            .max_v = min_v + col_factor, //FCS TODO: is this right?
        };

        return true;
    }
    return false;
}

//TODO: function that takes in a string and returns/appends to stretchy_buffers of the Vertices (4 per char) + Indices (6) for that string (with some input offset) (+ bounding box?)

void gui_free_font(GuiFont* in_font) {
    sb_free(in_font->image_data);
    *in_font = (GuiFont){};
}

//TODO: frame begin function (pass resolution, mouse state, etc...)
void gui_begin_frame(GuiContext* const context, GuiFrameState frame_state) {
    context->frame_state = frame_state;

    //Clear old vertex + index buffer data //TODO: don't realloc these arrays each frame
    sb_free(context->frame_state.vertices);
    sb_free(context->frame_state.indices);

    //FCS TODO: Remove
    printf("Mouse Coords: %i %i LMB: %s\n", context->frame_state.mouse_x, context->frame_state.mouse_y, context->frame_state.mouse_buttons[0] ? "DOWN" : "UP");
}

//TODO: make x,y, width, height int32_t?
bool gui_button(const GuiContext* const context, const char* label, float x, float y, float width, float height) {
    //TODO: return true if mouse_buttons[0], AND mouse_x > x and < width, AND mouse_y
    const GuiFrameState* const frame_state = &context->frame_state;
    bool button_clicked = frame_state->mouse_buttons[0] 
                       && frame_state->mouse_x > x 
                       && frame_state->mouse_x < x + width 
                       && frame_state->mouse_y > y 
                       && frame_state->mouse_y < y + height;

    Vec4 button_color = button_clicked ? vec4_new(1,0,0,1) : vec4_new(1,1,1,1);

    //FCS TODO: Vertex Data
    float res_x = (float)frame_state->res_x;
    float res_y = (float)frame_state->res_y;

    float normalized_x = x / res_x;
    float normalized_y = y / res_y;
    float normalized_width = width / res_x;
    float normalized_height = height / res_y;

    //TODO: "make box" helper
    //TODO: Helper to push normalized verts

    uint32_t next_vert_idx = sb_count(frame_state->vertices);

    //Indices
    sb_push(frame_state->indices, next_vert_idx + 0);
    sb_push(frame_state->indices, next_vert_idx + 1);
    sb_push(frame_state->indices, next_vert_idx + 2);

    sb_push(frame_state->indices, next_vert_idx + 1);
    sb_push(frame_state->indices, next_vert_idx + 2);
    sb_push(frame_state->indices, next_vert_idx + 3);

    //Vertices
    sb_push(frame_state->vertices, ((GuiVert) {
        .position = vec2_new(normalized_x, normalized_y),
        .uv = vec2_new(0,0), //TODO:
        .color = button_color,
    }));

    sb_push(frame_state->vertices, ((GuiVert) {
        .position = vec2_new(normalized_x + normalized_width, normalized_y),
        .uv = vec2_new(0,0), //TODO:
        .color = button_color,
    }));

    sb_push(frame_state->vertices, ((GuiVert) {
        .position = vec2_new(normalized_x, normalized_y + normalized_height),
        .uv = vec2_new(0,0), //TODO:
        .color = button_color,
    }));

    sb_push(frame_state->vertices, ((GuiVert) {
        .position = vec2_new(normalized_x + normalized_width, normalized_y + normalized_height),
        .uv = vec2_new(0,0), //TODO:
        .color = button_color,
    }));


    return button_clicked;
}