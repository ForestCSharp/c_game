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

typedef struct GuiRect {
    Vec2 position;
    Vec2 size;
} GuiRect;

typedef struct GuiFrameState {
    Vec2 window_size;
    Vec2 mouse_pos; //Mouse position, in screen coordinates
    bool mouse_buttons[3];
} GuiFrameState;

typedef struct GuiDrawData {
    sbuffer(GuiVert)  vertices;
    sbuffer(uint32_t) indices;
} GuiDrawData;

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
    GuiFrameState prev_frame_state;
    GuiDrawData draw_data;
} GuiContext;

typedef struct CharUVs {
    float min_u;
    float min_v;
    float max_u;
    float max_v;
} CharUVs;

typedef enum GuiClickState {
    GUI_RELEASED = 0,
    GUI_CLICKED  = 1,
    GUI_HELD     = 2,
} GuiClickState;

bool gui_is_point_in_rect(GuiRect in_rect, Vec2 in_point) {
    return in_point.x >= in_rect.position.x
        && in_point.x <= in_rect.position.x + in_rect.size.x
        && in_point.y >= in_rect.position.y
        && in_point.y <= in_rect.position.y + in_rect.size.y;
}

//TODO: take in a default font
void gui_init(GuiContext* out_context) {
    GuiFrameState default_frame_state = {
        .window_size = vec2_new(1920,1080),
        .mouse_pos = vec2_new(0,0),
        .mouse_buttons = {false, false, false},
    };

    *out_context = (GuiContext){
        .frame_state = default_frame_state,
        .prev_frame_state = default_frame_state,
        .draw_data = (GuiDrawData) {
            .vertices = NULL,
            .indices  = NULL,
        }
    };
}

void gui_shutdown(GuiContext* in_context) {
    sb_free(in_context->draw_data.vertices);
    sb_free(in_context->draw_data.indices);
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
    
    context->prev_frame_state = context->frame_state;
    context->frame_state = frame_state;

    //Clear old vertex + index buffer data //TODO: don't realloc these arrays each frame
    sb_free(context->draw_data.vertices);
    sb_free(context->draw_data.indices);
}

//TODO: UV-rect argument (for font-rendering)
/* Currently takes in screen-space coordinates and converts to [0,1] where (0,0) is top-left and (1,1) is bottom right */
void gui_make_box(const GuiContext* const in_context, const GuiRect* const in_rect, const Vec4* const in_color) {
    
    const Vec2* window_size = &in_context->frame_state.window_size;
    
    float normalized_x = in_rect->position.x / window_size->x;
    float normalized_y = in_rect->position.y / window_size->y;
    float normalized_width = in_rect->size.x / window_size->x;
    float normalized_height = in_rect->size.y / window_size->y;

    uint32_t next_vert_idx = sb_count(in_context->draw_data.vertices);

    //Indices
    sb_push(in_context->draw_data.indices, next_vert_idx + 0);
    sb_push(in_context->draw_data.indices, next_vert_idx + 1);
    sb_push(in_context->draw_data.indices, next_vert_idx + 2);

    sb_push(in_context->draw_data.indices, next_vert_idx + 1);
    sb_push(in_context->draw_data.indices, next_vert_idx + 2);
    sb_push(in_context->draw_data.indices, next_vert_idx + 3);

    //Vertices
    sb_push(in_context->draw_data.vertices, ((GuiVert) {
        .position = vec2_new(normalized_x, normalized_y),
        .uv = vec2_new(0,0), //TODO:
        .color = *in_color,
    }));

    sb_push(in_context->draw_data.vertices, ((GuiVert) {
        .position = vec2_new(normalized_x + normalized_width, normalized_y),
        .uv = vec2_new(0,0), //TODO:
        .color = *in_color,
    }));

    sb_push(in_context->draw_data.vertices, ((GuiVert) {
        .position = vec2_new(normalized_x, normalized_y + normalized_height),
        .uv = vec2_new(0,0), //TODO:
        .color = *in_color,
    }));

    sb_push(in_context->draw_data.vertices, ((GuiVert) {
        .position = vec2_new(normalized_x + normalized_width, normalized_y + normalized_height),
        .uv = vec2_new(0,0), //TODO:
        .color = *in_color,
    }));
}

GuiClickState gui_button(const GuiContext* const context, const char* label, float x, float y, float width, float height) {
    const GuiFrameState* const current_frame_state = &context->frame_state;
    const GuiFrameState* const prev_frame_state = &context->prev_frame_state;

    GuiRect button_rect = {
        .position = {
            .x = x,
            .y = y,
        },
        .size = {
            .x = width,
            .y = height,
        }
    };

    const bool cursor_currently_overlaps = gui_is_point_in_rect(button_rect, current_frame_state->mouse_pos);
    const bool cursor_previously_overlapped = gui_is_point_in_rect(button_rect, prev_frame_state->mouse_pos);

    //Held: we're over the button and holding LMB
    bool button_held = cursor_currently_overlaps && current_frame_state->mouse_buttons[0];

    //Clicked: we were previously holding this button and have now released it
    bool button_clicked = cursor_currently_overlaps && !current_frame_state->mouse_buttons[0]
                       && cursor_previously_overlapped && prev_frame_state->mouse_buttons[0];

    //Draw held buttons as red for now
    const Vec4 button_color = button_held ? vec4_new(1,0,0,1) : vec4_new(0,0,0,1);

    gui_make_box(context, &button_rect, &button_color);

    return button_clicked ? GUI_CLICKED : button_held ? GUI_HELD : GUI_RELEASED;
}