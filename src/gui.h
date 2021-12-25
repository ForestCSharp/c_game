#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "stretchy_buffer.h"
#include "vec.h"

/*
    Naming Conventions
    -> User-friendly 'free' control functions
        -> gui_<control>
        -> ex: gui_button, gui_text
    -> window based functions
        -> gui_window_<control>
        -> ex: gui_window_button
    -> internal functions
        -> gui_make_<control/primitive>
        -> ex: gui_make_button, gui_make_text

*/

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
    GuiFont default_font;
} GuiContext;

typedef enum GuiClickState {
    GUI_RELEASED = 0,
    GUI_CLICKED  = 1,
    GUI_HELD     = 2,
    GUI_HOVERED  = 3,
} GuiClickState;

typedef struct GuiWindow {
    char name[256];
    GuiRect window_rect;
    bool is_expanded;
    bool is_open;

    //Internal vars //TODO: store internal vars elsewhere (inner struct?)
    bool is_moving;  //Are we dragging a window (ignore top bar clicks)
    bool is_resizing; //Are we resizing the window
    uint32_t next_index;    //Next Index in window's list of controls
} GuiWindow;

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

bool gui_font_get_uvs(const GuiFont* const in_font, char in_char, GuiRect* out_uv_rect) {
   
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

        *out_uv_rect = (GuiRect) {
            .position = {
                .x = min_u,
                .y = min_v,
            },
            .size = {
                .x = col_factor,
                .y = col_factor,
            }
        };

        return true;
    }
    return false;
}

void gui_free_font(GuiFont* in_font) {
    sb_free(in_font->image_data);
    *in_font = (GuiFont){};
}

bool gui_rect_intersects_point(const GuiRect in_rect, const Vec2 in_point) {
    return in_point.x >= in_rect.position.x
        && in_point.x <= in_rect.position.x + in_rect.size.x
        && in_point.y >= in_rect.position.y
        && in_point.y <= in_rect.position.y + in_rect.size.y;
}

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

    if (!gui_load_font("data/fonts/JetBrainsMonoRegular.bff", &out_context->default_font)) {
		printf("failed to load default font\n");
		exit(1);
    }
}

void gui_shutdown(GuiContext* in_context) {
    sb_free(in_context->draw_data.vertices);
    sb_free(in_context->draw_data.indices);

    gui_free_font(&in_context->default_font);
}

//TODO: frame begin function (pass resolution, mouse state, etc...)
void gui_begin_frame(GuiContext* const in_context, GuiFrameState frame_state) {
    
    in_context->prev_frame_state = in_context->frame_state;
    in_context->frame_state = frame_state;

    //Clear old vertex + index buffer data //TODO: don't realloc these arrays each frame
    sb_free(in_context->draw_data.vertices);
    sb_free(in_context->draw_data.indices);
}

//TODO: UV-rect argument (for font-rendering)
/* Currently takes in screen-space coordinates (i.e in pixels) and converts to [0,1] range where (0,0) is top-left and (1,1) is bottom right */
void gui_make_box(const GuiContext* const in_context, const GuiRect* const in_rect, const Vec4* const in_color, const GuiRect* const in_uv_rect) {
    
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

    const Vec2 uv_start = in_uv_rect ? in_uv_rect->position : vec2_new(-1,-1);
    const Vec2 uv_size = in_uv_rect ? in_uv_rect->size : vec2_new(0,0);

    //Vertices
    sb_push(in_context->draw_data.vertices, ((GuiVert) {
        .position = vec2_new(normalized_x, normalized_y),
        .uv = uv_start,
        .color = *in_color,
    }));

    sb_push(in_context->draw_data.vertices, ((GuiVert) {
        .position = vec2_new(normalized_x + normalized_width, normalized_y),
        .uv = vec2_add(uv_start, vec2_new(uv_size.x, 0)),
        .color = *in_color,
    }));

    sb_push(in_context->draw_data.vertices, ((GuiVert) {
        .position = vec2_new(normalized_x, normalized_y + normalized_height),
        .uv = vec2_add(uv_start, vec2_new(0, uv_size.y)),
        .color = *in_color,
    }));

    sb_push(in_context->draw_data.vertices, ((GuiVert) {
        .position = vec2_new(normalized_x + normalized_width, normalized_y + normalized_height),
        .uv = vec2_add(uv_start, uv_size),
        .color = *in_color,
    }));
}

typedef enum GuiAlignment {
    GUI_ALIGN_LEFT,
    GUI_ALIGN_CENTER,
    // GUI_ALIGN_RIGHT, //TODO:
} GuiAlignment;

//TODO: simpler version of this that only takes in position arg
//TODO: if in_bounding_rect size is <= 0, consider that dimension unbounded
void gui_make_text(const GuiContext* const context, const char* in_text, const GuiRect* const in_bounding_rect, GuiAlignment in_alignment) {
    size_t num_chars = strlen(in_text);

    // const float initial_offset = 15.0f; //TODO: style setting
    const float char_size = 27.5f; //TODO: style setting
    const float spacing = -15.0f; //TODO: style setting

    //TODO: Ensure we have enough height for char_size

    //Compute max possible chars
    float bounding_rect_width = in_bounding_rect->size.x;

    //Compute baed on bounding rect width, but never greater than num_chars
    size_t max_possible_chars = min(bounding_rect_width > 0.f ? (bounding_rect_width / (char_size + spacing)) - 1 : num_chars, num_chars);
    float text_width = max_possible_chars * (char_size + spacing);

    Vec2 current_offset = in_bounding_rect->position;
    switch (in_alignment)
    {
        case GUI_ALIGN_LEFT:
        {
            current_offset = in_bounding_rect->position;
            break;
        }
        case GUI_ALIGN_CENTER:
        {
            float text_start_x = (in_bounding_rect->position.x + in_bounding_rect->size.x / 2.f) - text_width / 2.f;
            current_offset = vec2_new(text_start_x, in_bounding_rect->position.y);
            break;
        }
    }

    GuiRect text_rect = {
        .position = current_offset,
        .size = vec2_new(0.f, 0.f),
    };
    
    for (size_t i = 0; i < max_possible_chars; ++i) {
        
        char current_char = in_text[i];

        GuiRect uv_rect;
        gui_font_get_uvs(&context->default_font, current_char, &uv_rect);

        GuiRect box_rect = {
            .position = current_offset,
            .size = vec2_new(char_size, char_size),
        };

        Vec4 color = vec4_new(1,1,1,1); //TODO: arg
        gui_make_box(context, &box_rect, &color, &uv_rect);

        current_offset = vec2_add(current_offset, vec2_new(char_size + spacing, 0));
    }
}

//TODO: Alignment arg
void gui_text(const GuiContext* const in_context, const char* in_text, Vec2 position) {
    gui_make_text(in_context, in_text, &(GuiRect) {
        .position = position,
        .size = vec2_new(-1,-1),
    }, GUI_ALIGN_LEFT);
}

GuiClickState gui_make_button(const GuiContext* const in_context, const char* in_label, const GuiRect* const in_rect, const GuiAlignment in_alignment, bool is_active) {
    const GuiFrameState* const current_frame_state = &in_context->frame_state;
    const GuiFrameState* const prev_frame_state = &in_context->prev_frame_state;

    GuiRect button_rect = {
        .position = {
            .x = in_rect->position.x,
            .y = in_rect->position.y,
        },
        .size = {
            .x = in_rect->size.x,
            .y = in_rect->size.y,
        }
    };

    const bool cursor_currently_overlaps = gui_rect_intersects_point(button_rect, current_frame_state->mouse_pos);
    const bool cursor_previously_overlapped = gui_rect_intersects_point(button_rect, prev_frame_state->mouse_pos);

    //Held: we're over the button and holding LMB. 
    //Using previous frame data to prevent drags from losing our "HELD" state
    bool button_held = cursor_previously_overlapped && prev_frame_state->mouse_buttons[0];

    //Clicked: we were previously holding this button and have now released it
    bool button_clicked = cursor_currently_overlaps && !current_frame_state->mouse_buttons[0]
                       && cursor_previously_overlapped && prev_frame_state->mouse_buttons[0];

    //Draw held buttons as red for now
    const bool button_hovered = cursor_currently_overlaps;
    const float alpha = 0.75f;
    //TODO: refactor this
    GuiClickState out_click_state = !is_active ? GUI_RELEASED : button_clicked ? GUI_CLICKED : button_held ? GUI_HELD : button_hovered ? GUI_HOVERED : GUI_RELEASED;
    
    Vec4 button_color; //TODO: button style settings
    switch (out_click_state) {
        case GUI_RELEASED:
            button_color = vec4_new(0,0,0,alpha);
            break;
        case GUI_HOVERED:
            button_color = vec4_new(0.5, 0,0, alpha);
            break;
        case GUI_HELD:
            button_color = vec4_new(1,0,0,1);
            break;
        default: 
            button_color = vec4_new(0,0,0,alpha);
    }

    gui_make_box(in_context, &button_rect, &button_color, NULL);

    Vec4 text_color = vec4_new(1,1,1,alpha); //TODO: input arg
    const float button_text_padding = 15.0f; //TODO: Style setting

    GuiRect text_rect = button_rect;
    text_rect.position.x += button_text_padding;
    text_rect.size.x -= (button_text_padding * 2.f);
    gui_make_text(in_context, in_label, &text_rect, in_alignment);

    return out_click_state;
}

//TODO: replace 4 args with 2 vecs
GuiClickState gui_button(const GuiContext* const in_context, const char* in_label, float x, float y, float width, float height) {
    return gui_make_button(in_context, in_label, &(GuiRect) {
        .position = {
            .x = x,
            .y = y,
        },
        .size = {
            .x = width,
            .y = height,
        }
    },
    GUI_ALIGN_CENTER, true);
}

static const float top_bar_height = 35.0f; //TODO: style setting

void gui_begin_window(GuiContext* const in_context, GuiWindow* const in_window) {

    in_window->next_index = 0;
    
    GuiRect* const window_rect = &in_window->window_rect;

    //Main window area
    const Vec4 window_color = vec4_new(.2, .2, .2, .9); //TODO: Style setting
    if (in_window->is_expanded) {
        gui_make_box(in_context, window_rect, &window_color, NULL);
    }

    //Top Bar
    const Vec4 top_bar_color = vec4_new(.8, .8, .8, .8); //TODO: Style setting
    GuiRect top_bar_rect = {
        .position = window_rect->position,
        .size = {
            .x = window_rect->size.x,
            .y = top_bar_height,
        }
    };

    GuiClickState top_bar_click_state = gui_make_button(in_context, in_window->name, &top_bar_rect, GUI_ALIGN_LEFT, !in_window->is_resizing);
    if (top_bar_click_state == GUI_CLICKED) {
        if (in_window->is_moving) {
            in_window->is_moving = false;
        }
        else {
            in_window->is_expanded = !in_window->is_expanded;
        }
    }

    //Window moving
    Vec2 mouse_pos = in_context->frame_state.mouse_pos;
    Vec2 prev_mouse_pos = in_context->prev_frame_state.mouse_pos;
    Vec2 mouse_delta = vec2_sub(mouse_pos, prev_mouse_pos);

    const bool is_dragging_window = top_bar_click_state == GUI_HELD;
    if (is_dragging_window && vec2_length_squared(mouse_delta) > 0.0f) {
        window_rect->position = vec2_add(window_rect->position, mouse_delta);
        //TODO: make sure top-bar is always slightly visible
        in_window->is_moving = true;
    }

    //Resize Control bottom right
    if (in_window->is_expanded) {
        const Vec2 resize_control_size = vec2_new(15.f, 15.f);
        const Vec2 resize_control_position = vec2_sub(vec2_add(window_rect->position, window_rect->size), resize_control_size);

        const GuiRect resize_control_rect = {
            .position = resize_control_position,
            .size = resize_control_size,
        };

        in_window->is_resizing = gui_make_button(in_context, "", &resize_control_rect, GUI_ALIGN_CENTER, !in_window->is_moving) == GUI_HELD;
        if (in_window->is_resizing) {
            window_rect->size = vec2_add(window_rect->size, mouse_delta);

            float minimum_window_height = top_bar_height + resize_control_size.y;
            window_rect->size.y = max(minimum_window_height, window_rect->size.y);
        }
    }

    //TODO: Need to cache current window in context
}

static const float window_row_padding_y = 2.5f; //TODO: Style setting
static const float window_row_height = 35.0f;  //TODO: Style setting

//Attemps to allocate space for a new control in 'in_window'. If there is space, out_rect is written and true is returned.
bool gui_window_compute_control_rect(GuiWindow* const in_window, GuiRect* out_rect) {
    //Store and increment control index
    const uint32_t control_index = in_window->next_index++;
    
    const GuiRect* const window_rect = &in_window->window_rect;
    const Vec2 window_pos = window_rect->position;
    const Vec2 window_size = window_rect->size;

    //Need padding after first element as well, so add 1 to control index to account for that
    const float y_offset = window_pos.y + top_bar_height + ((window_row_height) * (float)control_index) + (window_row_padding_y * (float) (control_index + 1));

    bool has_space_for_control = y_offset + window_row_height <= window_rect->position.y + window_rect->size.y;

    if (has_space_for_control) {
        *out_rect = (GuiRect) {
            .position = {
                .x = window_pos.x,
                .y = y_offset,
            },
            .size = {
                .x = window_size.x,
                .y = window_row_height,
            },
        };
    }

    return has_space_for_control;
}

GuiClickState gui_window_button(const GuiContext* const in_context, GuiWindow* const in_window, const char* in_label) {
    
    GuiClickState out_click_state = GUI_RELEASED;

    if (in_window->is_expanded) {
        const GuiRect* const window_rect = &in_window->window_rect;

        GuiRect button_rect;
        if (gui_window_compute_control_rect(in_window, &button_rect)) {
            out_click_state = gui_make_button(in_context, in_label, &button_rect, GUI_ALIGN_CENTER, !in_window->is_resizing);
        }
    }

    return out_click_state;
}