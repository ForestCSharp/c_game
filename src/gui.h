#pragma once
#include "basic_math.h"
#include "stretchy_buffer.h"
#include "types.h"
#include "vec.h"

/*
    Naming Conventions
    -> User-friendly 'free' control functions
        -> gui_<control>
        -> ex:
 * gui_button, gui_text
    -> window based functions
        -> gui_window_<control>
        -> ex: gui_window_button

 * -> internal functions
        -> gui_make_<control/primitive>
        -> ex: gui_make_button, gui_make_text
    ->
 * low-level drawing functions
        -> gui_draw_<primitive>
        -> doesn't depend on context, only takes in
 * draw_data
*/

typedef struct GuiVert
{
    Vec2 position;
    Vec2 uv;
    Vec4 color;
    i32 has_texture;
} GuiVert;

typedef struct GuiRect
{
    Vec2 position;
    Vec2 size;
    u32 z_order;
} GuiRect;

typedef struct GuiWindow
{
    char name[256];
    GuiRect window_rect;
    float scroll_amount;
    bool is_expanded;
    bool is_open;
    bool is_resizable;
    bool is_movable;

    // Internal Vars
    bool is_moving; // Are we currently dragging this window
    bool is_resizing; // Are we currently resizing this window
    bool is_scrolling; // Are we currently scrolling this window
    u32 num_controls; // Next Index in window's list of controls

    // GuiDrawData draw_data;  //Stores window's local draw data //TODO:
} GuiWindow;

typedef struct GuiFrameState
{
    Vec2 screen_size;
    Vec2 mouse_pos;
    bool mouse_buttons[3];
    sbuffer(GuiWindow) open_windows; // FCS TODO: Store all active window data (can check if is_moving or is_resizing)
} GuiFrameState;

typedef struct GuiDrawData
{
    sbuffer(GuiVert) vertices;
    sbuffer(u32) indices;
} GuiDrawData;

#define BFF_ID 0xF2BF

typedef enum GuiFontType
{
    FONT_TYPE_ALPHA = 8,
    FONT_TYPE_RGB = 24,
    FONT_TYPE_RGBA = 32,
} GuiFontType;

typedef struct GuiFont
{
    u32 image_width;
    u32 image_height;
    u32 cell_width;
    u32 cell_height;
    GuiFontType font_type;
    char first_char;
    u8 char_widths[256];
    sbuffer(u8) image_data;
    u64 image_data_size;
} GuiFont;

typedef struct GuiContext
{
    GuiFrameState frame_state;
    GuiFrameState prev_frame_state;
    GuiDrawData draw_data;
    GuiFont default_font;
} GuiContext;

typedef enum GuiClickState
{
    GUI_RELEASED,
    GUI_HOVERED,
    GUI_HELD,
    GUI_CLICKED,
} GuiClickState;

typedef enum GuiAlignment
{
    GUI_ALIGN_LEFT,
    GUI_ALIGN_CENTER,
    // GUI_ALIGN_RIGHT, //TODO:
} GuiAlignment;

typedef struct GuiButtonStyleArgs
{
    Vec4 released_color;
    Vec4 hovered_color;
    Vec4 held_color;
    GuiAlignment text_alignment;
    float text_padding;
} GuiButtonStyleArgs;

static const GuiButtonStyleArgs default_button_style = {
    .released_color = {0.0, 0.0, 0.0, 0.75f},
    .hovered_color = {0.4, 0.0, 0.0, 0.75f},
    .held_color = {1.0, 0.0, 0.0, 1.0},
    .text_alignment = GUI_ALIGN_CENTER,
    .text_padding = 15.0f,
};

Vec4 gui_style_get_button_color(const GuiButtonStyleArgs* const in_style_args, const GuiClickState in_click_state)
{
    switch (in_click_state)
    {
    case GUI_RELEASED:
        return in_style_args->released_color;
    case GUI_HOVERED:
        return in_style_args->hovered_color;
    case GUI_HELD:
    case GUI_CLICKED:
        return in_style_args->held_color;
    default:
        return in_style_args->released_color;
    }
}

bool gui_load_font(const char* filename, GuiFont* out_font)
{
    FILE* file = fopen(filename, "rb");

    if (file && out_font)
    {
        *out_font = (GuiFont){};

        u16 bff_id;
        if (fread(&bff_id, 2, 1, file) != 1 || bff_id != BFF_ID)
        {
            return false;
        }
        if (fread(&out_font->image_width, 4, 1, file) != 1)
        {
            return false;
        }
        if (fread(&out_font->image_height, 4, 1, file) != 1)
        {
            return false;
        }
        if (fread(&out_font->cell_width, 4, 1, file) != 1)
        {
            return false;
        }
        if (fread(&out_font->cell_height, 4, 1, file) != 1)
        {
            return false;
        }

        u8 font_type;
        if (fread(&font_type, 1, 1, file) == 1)
        {
            out_font->font_type = font_type;
        }
        else
        {
            return false;
        }
        if (fread(&out_font->first_char, 1, 1, file) != 1)
        {
            return false;
        }
        if (fread(out_font->char_widths, 1, 256, file) != 256)
        {
            return false;
        }

        // Finally, load actual image data
        out_font->image_data_size = out_font->image_width * out_font->image_height * (font_type / 8);

        sb_add(out_font->image_data, out_font->image_data_size);
        if (fread(out_font->image_data, 1, out_font->image_data_size, file) != out_font->image_data_size)
        {
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

bool gui_font_get_uvs(const GuiFont* const in_font, char in_char, GuiRect* out_uv_rect)
{

    i32 chars_per_row = in_font->image_width / in_font->cell_width;
    i32 chars_per_col = in_font->image_height / in_font->cell_height;
    i32 total_chars = chars_per_row * chars_per_col;

    if (in_char >= in_font->first_char && in_char <= in_font->first_char + total_chars)
    {
        i32 row = (in_char - in_font->first_char) / chars_per_row;
        i32 col = (in_char - in_font->first_char) - (row * chars_per_row);

        float row_factor = (float) in_font->cell_height / (float) in_font->image_height;
        float col_factor = (float) in_font->cell_width / (float) in_font->image_width;

        float min_u = col * col_factor;
        float min_v = row * row_factor;

        *out_uv_rect = (GuiRect){.position =
                                     {
                                         .x = min_u,
                                         .y = min_v,
                                     },
            .size = {
                .x = col_factor,
                .y = col_factor,
            }};

        return true;
    }
    return false;
}

void gui_free_font(GuiFont* in_font)
{
    sb_free(in_font->image_data);
    *in_font = (GuiFont){};
}

bool gui_rect_intersects_point(const GuiRect in_rect, const Vec2 in_point)
{
    return in_point.x >= in_rect.position.x && in_point.x <= in_rect.position.x + in_rect.size.x && in_point.y >= in_rect.position.y && in_point.y <= in_rect.position.y + in_rect.size.y;
}

GuiRect gui_rect_scale(const GuiRect in_rect, const Vec2 in_scale)
{
    return (GuiRect){.position =
                         {
                             .x = in_rect.position.x * in_scale.x,
                             .y = in_rect.position.y * in_scale.y,
                         },
        .size = {
            .x = in_rect.size.x * in_scale.x,
            .y = in_rect.size.y * in_scale.y,
        }};
}

void gui_init(GuiContext* out_context)
{
    GuiFrameState default_frame_state = {
        .screen_size = vec2_new(1920, 1080),
        .mouse_pos = vec2_new(0, 0),
        .mouse_buttons = {false, false, false},
        .open_windows = NULL,
    };

    *out_context = (GuiContext){.frame_state = default_frame_state,
        .prev_frame_state = default_frame_state,
        .draw_data = (GuiDrawData){
            .vertices = NULL,
            .indices = NULL,
        }};

    if (!gui_load_font("data/fonts/JetBrainsMonoRegular.bff", &out_context->default_font))
    {
        printf("failed to load default font\n");
        exit(1);
    }
}

void gui_shutdown(GuiContext* in_context)
{
    sb_free(in_context->draw_data.vertices);
    sb_free(in_context->draw_data.indices);
    sb_free(in_context->frame_state.open_windows);
    sb_free(in_context->prev_frame_state.open_windows);

    gui_free_font(&in_context->default_font);
}

void gui_begin_frame(GuiContext* const in_context, GuiFrameState frame_state)
{

    // Clean up previous frame state before reassign
    sb_free(in_context->prev_frame_state.open_windows);
    in_context->prev_frame_state = in_context->frame_state;
    in_context->frame_state = frame_state;

    // Clear old vertex + index buffer data
    // TODO: don't realloc these arrays each frame, need sb_reset that clears count but not capacity
    sb_free(in_context->draw_data.vertices);
    sb_free(in_context->draw_data.indices);
}

GuiRect gui_make_fullscreen_rect(const GuiContext* const in_context, const u32 in_z_order)
{
    return (GuiRect){
        .position =
            {
                .x = 0,
                .y = 0,
            },
        .size = in_context->frame_state.screen_size,
        .z_order = in_z_order,
    };
}

/* positions: [0,1] range */
void gui_draw_tri(GuiDrawData* const in_draw_data, const GuiVert vertices[3])
{

    u32 next_vert_idx = sb_count(in_draw_data->vertices);

    // Indices
    sb_push(in_draw_data->indices, next_vert_idx + 0);
    sb_push(in_draw_data->indices, next_vert_idx + 1);
    sb_push(in_draw_data->indices, next_vert_idx + 2);

    // Vertices
    sb_push(in_draw_data->vertices, vertices[0]);
    sb_push(in_draw_data->vertices, vertices[1]);
    sb_push(in_draw_data->vertices, vertices[2]);
}

/* positions: [0,1] range */
void gui_draw_quad(GuiDrawData* const in_draw_data, const GuiVert vertices[4])
{

    u32 next_vert_idx = sb_count(in_draw_data->vertices);

    // Indices
    sb_push(in_draw_data->indices, next_vert_idx + 0);
    sb_push(in_draw_data->indices, next_vert_idx + 1);
    sb_push(in_draw_data->indices, next_vert_idx + 2);

    sb_push(in_draw_data->indices, next_vert_idx + 1);
    sb_push(in_draw_data->indices, next_vert_idx + 2);
    sb_push(in_draw_data->indices, next_vert_idx + 3);

    // Vertices
    sb_push(in_draw_data->vertices, vertices[0]);
    sb_push(in_draw_data->vertices, vertices[1]);
    sb_push(in_draw_data->vertices, vertices[2]);
    sb_push(in_draw_data->vertices, vertices[3]);
}

/* Currently takes in screen-space coordinates (i.e in pixels) and converts to [0,1] range where (0,0) is top-left and

 * * (1,1) is bottom right */
void gui_draw_box(GuiDrawData* const in_draw_data, const GuiRect* const in_rect, const Vec4* const in_color, const GuiRect* const in_uv_rect)
{

    const bool has_uvs = in_uv_rect != NULL;
    const Vec2 position = in_rect->position;
    const Vec2 size = in_rect->size;

    const Vec2 uv_start = in_uv_rect ? in_uv_rect->position : vec2_new(-1, -1);
    const Vec2 uv_size = in_uv_rect ? in_uv_rect->size : vec2_new(0, 0);

    gui_draw_quad(in_draw_data, (GuiVert[4]){
                                    (GuiVert){
                                        .position = position,
                                        .uv = uv_start,
                                        .color = *in_color,
                                        .has_texture = has_uvs,
                                    },
                                    (GuiVert){
                                        .position = vec2_add(position, vec2_new(size.x, 0)),
                                        .uv = vec2_add(uv_start, vec2_new(uv_size.x, 0)),
                                        .color = *in_color,
                                        .has_texture = has_uvs,
                                    },
                                    (GuiVert){
                                        .position = vec2_add(position, vec2_new(0, size.y)),
                                        .uv = vec2_add(uv_start, vec2_new(0, uv_size.y)),
                                        .color = *in_color,
                                        .has_texture = has_uvs,
                                    },
                                    (GuiVert){
                                        .position = vec2_add(position, size),
                                        .uv = vec2_add(uv_start, uv_size),
                                        .color = *in_color,
                                        .has_texture = has_uvs,
                                    },
                                });
}

Vec2 recursive_bezier(const float t, const u32 num_points, const Vec2* in_points)
{
    assert(num_points > 0);

    switch (num_points)
    {
    case 1:
        return in_points[0];
    default:
        return vec2_lerp(t, recursive_bezier(t, num_points - 1, in_points), recursive_bezier(t, num_points - 1, in_points + 1));
    }
}

Vec2 recursive_bezier_deriv(const float t, const u32 num_points, const Vec2* in_points)
{
    assert(num_points > 0);

    switch (num_points)
    {
    case 1:
        {
            return vec2_new(0.0f, 0.0f);
        }
    default:
        {
            const Vec2 left = recursive_bezier(t, num_points - 1, in_points);
            const Vec2 right = recursive_bezier(t, num_points - 1, in_points + 1);
            return vec2_scale(vec2_sub(left, right), num_points);
        }
    }
}

void gui_draw_bezier(GuiDrawData* const in_draw_data, const u32 num_points, Vec2* points, const u32 num_samples, const Vec4 color, const float width)
{
    assert(num_points > 0);
    assert(num_samples > 0);

    const float step_amount = 1.0f / (float) num_samples;

    float current_t = 0.0f;

    for (u32 current_idx = 0; current_idx < num_samples; ++current_idx)
    {
        const float next_t = current_t + step_amount;

        const Vec2 current_pos = recursive_bezier(current_t, num_points, points);
        const Vec2 next_pos = recursive_bezier(next_t, num_points, points);

        const Vec2 current_tangent = recursive_bezier_deriv(current_t, num_points, points);
        const Vec2 next_tangent = recursive_bezier_deriv(next_t, num_points, points);

        const Vec2 current_normal = vec2_normalize(vec2_rotate(current_tangent, degrees_to_radians(-90)));
        const Vec2 next_normal = vec2_normalize(vec2_rotate(next_tangent, degrees_to_radians(-90)));

        const Vec2 invalid_uv = vec2_new(-1, -1);

        const Vec2 pos_a = vec2_add(current_pos, vec2_scale(current_normal, width));
        const Vec2 pos_b = vec2_add(current_pos, vec2_scale(current_normal, -width));

        const Vec2 pos_c = vec2_add(next_pos, vec2_scale(next_normal, width));
        const Vec2 pos_d = vec2_add(next_pos, vec2_scale(next_normal, -width));

        gui_draw_quad(in_draw_data, (GuiVert[4]){
                                        (GuiVert){
                                            .position = pos_a,
                                            .uv = invalid_uv,
                                            .color = color,
                                        },
                                        (GuiVert){
                                            .position = pos_b,
                                            .uv = invalid_uv,
                                            .color = color,
                                        },
                                        (GuiVert){
                                            .position = pos_c,
                                            .uv = invalid_uv,
                                            .color = color,
                                        },
                                        (GuiVert){
                                            .position = pos_d,
                                            .uv = invalid_uv,
                                            .color = color,
                                        },
                                    });

        current_t += step_amount;
    }
}

/* Same as above, but in screen-space */
void gui_bezier(GuiContext* const in_context, const u32 num_points, Vec2* points, const u32 num_samples, const Vec4 color, const float width)
{
    Vec2 normalized_points[num_points];
    memcpy(normalized_points, points, sizeof(Vec2) * num_points);

    const Vec2 window_size = in_context->frame_state.screen_size;
    for (u32 i = 0; i < num_points; ++i)
    {
        normalized_points[i].x /= window_size.x;
        normalized_points[i].y /= window_size.y;
    }

    gui_draw_bezier(&in_context->draw_data, num_points, normalized_points, num_samples, color, width);
}

// TODO: Add GuiTextStyleArgs (replace GuiAlignment arg (GuiAlignment should live in style args))
void gui_make_text(GuiContext* const in_context, const char* in_text, const GuiRect* const in_bounding_rect, GuiAlignment in_alignment)
{

    if (in_bounding_rect->size.x > 0.0f)
    {
        size_t num_chars = strlen(in_text);

        const float char_size = 22.5f; // TODO: style setting
        const float spacing = -12.0f; // TODO: style setting

        // TODO: Ensure we have enough height for char_size

        // Compute max possible chars
        float bounding_rect_width = in_bounding_rect->size.x;

        // Compute baed on bounding rect width, but never greater than num_chars
        size_t max_possible_chars = MIN(bounding_rect_width > 0.f ? (bounding_rect_width / (char_size + spacing)) - 1 : num_chars, num_chars);
        float text_width = max_possible_chars * (char_size + spacing);

        Vec2 current_offset;
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

        // Center of rect on y-axis
        current_offset.y += (in_bounding_rect->size.y - char_size) / 2.0f;

        GuiRect text_rect = {
            .position = current_offset,
            .size = vec2_new(0.f, 0.f),
        };

        for (size_t i = 0; i < max_possible_chars; ++i)
        {

            char current_char = in_text[i];

            GuiRect uv_rect;
            gui_font_get_uvs(&in_context->default_font, current_char, &uv_rect);

            GuiRect box_rect = gui_rect_scale(
                (GuiRect){
                    .position = current_offset,
                    .size = vec2_new(char_size, char_size),
                },
                float_div_vec2(1.0, in_context->frame_state.screen_size));

            Vec4 text_color = vec4_new(1, 1, 1, 1); // TODO: arg
            gui_draw_box(&in_context->draw_data, &box_rect, &text_color, &uv_rect);

            current_offset = vec2_add(current_offset, vec2_new(char_size + spacing, 0));
        }
    }
}

void gui_text(GuiContext* const in_context, const char* in_text, Vec2 position, Vec2 size, GuiAlignment alignment)
{
    gui_make_text(in_context, in_text,
        &(GuiRect){
            .position = position,
            .size = size,
        },
        alignment);
}

// Hit-Test with previous-frame geometry: fail if we intersect with a higher z-order window OR a higher z-order window
// is moving/resizing FCS TODO: Make sure to handle is_expanded
bool gui_hit_test(const GuiContext* const in_context, const GuiRect in_rect)
{
    for (u32 i = 0; i < sb_count(in_context->prev_frame_state.open_windows); ++i)
    {
        const GuiWindow current_window = in_context->prev_frame_state.open_windows[i];
        const GuiRect window_rect = current_window.window_rect;
        const bool current_window_is_busy = current_window.is_moving || current_window.is_resizing || current_window.is_scrolling;
        if (in_rect.z_order < window_rect.z_order && (current_window_is_busy || gui_rect_intersects_point(window_rect, in_context->frame_state.mouse_pos)))
        {
            return false;
        }
    }
    return true;
}

// If in_style_args is nullptr, this button is "invisible" but can still be used for hit-testing
GuiClickState gui_make_button(
    GuiContext* const in_context, const char* in_label, const GuiRect* const in_draw_rect, const GuiRect* const in_hit_rect, const GuiButtonStyleArgs* in_style_args, const bool is_active)
{
    const GuiFrameState* const current_frame_state = &in_context->frame_state;
    const GuiFrameState* const prev_frame_state = &in_context->prev_frame_state;

    GuiRect hit_rect = *in_hit_rect; // TODO: use in_hit_rect if non-null

    bool cursor_currently_overlaps = gui_rect_intersects_point(hit_rect, current_frame_state->mouse_pos);
    bool cursor_previously_overlapped = gui_rect_intersects_point(hit_rect, prev_frame_state->mouse_pos);

    if (!gui_hit_test(in_context, hit_rect))
    {
        cursor_currently_overlaps = false;
        cursor_previously_overlapped = false;
    }

    // Held: we're over the button and holding LMB.
    // Using previous frame data to prevent drags from losing our "HELD" state
    bool button_held = cursor_previously_overlapped && prev_frame_state->mouse_buttons[0];

    // Clicked: we were previously holding this button and have now released it
    bool button_clicked = cursor_currently_overlaps && !current_frame_state->mouse_buttons[0] && cursor_previously_overlapped && prev_frame_state->mouse_buttons[0];

    // Draw held buttons as red for now
    const bool button_hovered = cursor_currently_overlaps;

    GuiClickState out_click_state = !is_active ? GUI_RELEASED : button_clicked ? GUI_CLICKED : button_held ? GUI_HELD : button_hovered ? GUI_HOVERED : GUI_RELEASED;

    if (in_style_args)
    {
        Vec4 button_color = gui_style_get_button_color(in_style_args, out_click_state);

        const GuiRect normalized_draw_rect = gui_rect_scale(*in_draw_rect, float_div_vec2(1.0, in_context->frame_state.screen_size));
        gui_draw_box(&in_context->draw_data, &normalized_draw_rect, &button_color, NULL);

        GuiRect text_rect = *in_draw_rect;
        text_rect.position.x += in_style_args->text_padding;
        text_rect.size.x -= (in_style_args->text_padding * 2.f);
        gui_make_text(in_context, in_label, &text_rect, in_style_args->text_alignment);
    }

    return out_click_state;
}

GuiClickState gui_button(GuiContext* const in_context, const char* in_label, Vec2 in_position, const Vec2 in_size)
{
    const Vec2 current_mouse_pos = in_context->frame_state.mouse_pos;
    const GuiRect rect = {
        .position = in_position,
        .size = in_size,
    };
    return gui_make_button(in_context, in_label, &rect, &rect, &default_button_style, true);
}

GuiClickState gui_make_slider_float(GuiContext* const in_context, float* const data_ptr, const Vec2 in_slider_bounds, const char* in_label, const GuiRect* const in_rect, const bool is_active)
{

    GuiButtonStyleArgs slider_button_style = default_button_style;
    slider_button_style.held_color = default_button_style.hovered_color;

    GuiClickState clicked_state = gui_make_button(in_context, "", in_rect, in_rect, &slider_button_style, is_active);
    if (clicked_state == GUI_HELD)
    {
        const float current_mouse_x = in_context->frame_state.mouse_pos.x;
        const float new_value = remap_clamped((current_mouse_x - in_rect->position.x) / in_rect->size.x, 0.0, 1.0, in_slider_bounds.x, in_slider_bounds.y);
        *data_ptr = new_value;
    }

    const float current_percent = remap_clamped(*data_ptr, in_slider_bounds.x, in_slider_bounds.y, 0.0, 1.0);
    const float filled_width = in_rect->size.x * current_percent;
    GuiRect normalized_filled_rect = gui_rect_scale(
        (GuiRect){
            .position = in_rect->position,
            .size =
                {
                    .x = filled_width,
                    .y = in_rect->size.y,
                },
        },
        float_div_vec2(1.0, in_context->frame_state.screen_size));

    const GuiButtonStyleArgs slider_filled_style = {
        .released_color = default_button_style.hovered_color,
        .hovered_color = vec4_lerp(0.5, default_button_style.hovered_color, default_button_style.held_color),
        .held_color = default_button_style.held_color,
    };

    const Vec4 slider_filled_color = gui_style_get_button_color(&slider_filled_style, clicked_state);
    gui_draw_box(&in_context->draw_data, &normalized_filled_rect, &slider_filled_color, NULL);

    // Draw label and current value text
    const char* format_string = "%s: %.3f";
    int required_chars = snprintf(NULL, 0, format_string, in_label, *data_ptr);
    char data_as_string[required_chars];
    snprintf(data_as_string, required_chars, format_string, in_label, *data_ptr);
    gui_make_text(in_context, data_as_string, in_rect, GUI_ALIGN_CENTER);

    return clicked_state;
}

GuiClickState gui_slider_float(GuiContext* const in_context, float* const data_ptr, const Vec2 in_slider_bounds, const char* in_label, const Vec2 in_position, const Vec2 in_size)
{
    return gui_make_slider_float(in_context, data_ptr, in_slider_bounds, in_label,
        &(GuiRect){
            .position = in_position,
            .size = in_size,
        },
        true);
}

// TODO: GuiWindowStyleArgs //TODO: Style Struct
static const float window_top_bar_height = 35.0f;
static const float window_row_padding_y = 2.5f;
static const float window_row_height = 35.0f;
static const float min_window_width = 50.0f;
static const float window_scrollbar_width = 20.0f;
static const float window_scrollbar_height = 40.0f;
static const Vec2 window_resize_control_size = {15.f, 15.f};
static const Vec4 window_color = {.2, .2, .2, .9};
static const bool window_allow_overscroll = false;

// TODO: Window z-order ideas
//  1. All windows in front of "free" controls
//  2. 'Active' Window gets drawn last
//  How? Store vertices/indices separately from free verts indices?

void gui_window_begin(GuiContext* const in_context, GuiWindow* const in_window)
{
    if (in_window->is_open)
    {
        in_window->num_controls = 0;

        GuiRect* const window_rect = &in_window->window_rect;

        // Main window area
        if (in_window->is_expanded)
        {
            const GuiRect normalized_window_rect = gui_rect_scale(*window_rect, float_div_vec2(1.0, in_context->frame_state.screen_size));
            gui_draw_box(&in_context->draw_data, &normalized_window_rect, &window_color, NULL);
        }

        // Top Bar
        GuiRect top_bar_rect = {
            .position = window_rect->position,
            .size =
                {
                    .x = window_rect->size.x,
                    .y = window_top_bar_height,
                },
            .z_order = window_rect->z_order,
        };

        GuiButtonStyleArgs top_bar_button_style = default_button_style;
        top_bar_button_style.text_alignment = GUI_ALIGN_LEFT;

        GuiClickState top_bar_click_state = gui_make_button(in_context, in_window->name, &top_bar_rect, &top_bar_rect, &top_bar_button_style, !in_window->is_resizing);
        if (top_bar_click_state == GUI_CLICKED)
        {
            if (in_window->is_moving)
            {
                in_window->is_moving = false;
            }
            else
            {
                in_window->is_expanded = !in_window->is_expanded;
            }
        }

        Vec2 mouse_pos = in_context->frame_state.mouse_pos;
        Vec2 prev_mouse_pos = in_context->prev_frame_state.mouse_pos;
        Vec2 mouse_delta = vec2_sub(mouse_pos, prev_mouse_pos);

        // Window moving
        if (in_window->is_movable)
        {
            const bool is_dragging_window = top_bar_click_state == GUI_HELD;
            if (is_dragging_window && vec2_length_squared(mouse_delta) > 0.0f)
            {
                window_rect->position = vec2_add(window_rect->position, mouse_delta);
                // TODO: make sure top-bar is always slightly visible
                in_window->is_moving = true;
            }
        }
    }
}

// Attemps to allocate space for a new control in 'in_window'. If there is space, out_rect is written and true is
// returned.
bool gui_window_compute_control_rect(GuiWindow* const in_window, GuiRect* out_rect)
{
    bool out_success = false;
    if (in_window->is_open && in_window->is_expanded)
    {
        // Store and increment control index
        const u32 control_index = in_window->num_controls++;

        // FCS TODO: Scrolling math
        const u32 first_visible_index = (u32) in_window->scroll_amount;
        // const float first_visible_offset 322_fractional(in_window->scroll_amount);
        const u32 max_visible_controls = (in_window->window_rect.size.y - window_top_bar_height) / (window_row_height + window_row_padding_y);
        const u32 last_visible_index = first_visible_index + max_visible_controls;

        const GuiRect* const window_rect = &in_window->window_rect;
        const Vec2 window_pos = window_rect->position;
        const Vec2 window_size = window_rect->size;

        // Need padding after first element as well, so add 1 to control index to account for that
        float y_offset = window_pos.y + window_top_bar_height + ((window_row_height) * (float) control_index) + (window_row_padding_y * (float) (control_index + 1));
        y_offset -= (window_row_height + window_row_padding_y) * first_visible_index;

        if (control_index >= first_visible_index && control_index < last_visible_index)
        {
            *out_rect = (GuiRect){
                .position =
                    {
                        .x = window_pos.x,
                        .y = y_offset,
                    },
                .size =
                    {
                        .x = window_size.x,
                        .y = window_row_height,
                    },
                .z_order = window_rect->z_order,
            };
            out_success = true;
        }
    }
    return out_success;
}

GuiClickState gui_window_button(GuiContext* const in_context, GuiWindow* const in_window, const char* in_label)
{

    GuiClickState out_click_state = GUI_RELEASED;

    GuiRect button_rect;
    if (gui_window_compute_control_rect(in_window, &button_rect))
    {
        out_click_state = gui_make_button(in_context, in_label, &button_rect, &button_rect, &default_button_style, !in_window->is_resizing);
    }

    return out_click_state;
}

GuiClickState gui_window_slider_float(GuiContext* const in_context, GuiWindow* const in_window, float* const data_ptr, const Vec2 in_slider_bounds, const char* in_label)
{
    GuiClickState out_click_state = GUI_RELEASED;

    GuiRect button_rect;
    if (gui_window_compute_control_rect(in_window, &button_rect))
    {
        out_click_state = gui_make_slider_float(in_context, data_ptr, in_slider_bounds, in_label, &button_rect, !in_window->is_resizing);
    }

    return out_click_state;
}

void gui_window_end(GuiContext* const in_context, GuiWindow* const in_window)
{

    if (in_window->is_open)
    {
        // Push at end so we have updated rect from any moves/resizes
        sb_push(in_context->frame_state.open_windows, *in_window);

        if (in_window->is_expanded)
        {
            GuiRect* const window_rect = &in_window->window_rect;

            Vec2 mouse_pos = in_context->frame_state.mouse_pos;
            Vec2 prev_mouse_pos = in_context->prev_frame_state.mouse_pos;
            Vec2 mouse_delta = vec2_sub(mouse_pos, prev_mouse_pos);

            const float scroll_area_length = window_rect->size.y - window_top_bar_height - window_resize_control_size.y;
            const bool has_space_for_scrollbar = scroll_area_length > window_scrollbar_height;
            if (has_space_for_scrollbar)
            { // Scrollbar FCS TODO: only draw if we need to be able to scroll

                // TODO: option to allow overscrolling
                const u32 max_visible_controls = (window_rect->size.y - window_top_bar_height) / (window_row_height + window_row_padding_y);
                const float max_scroll_amount = (float) MAX(0, in_window->num_controls - (window_allow_overscroll ? 1 : max_visible_controls));
                in_window->scroll_amount = CLAMP(in_window->scroll_amount, 0.0, max_scroll_amount);

                const float scrollbar_x = window_rect->position.x + window_rect->size.x - window_scrollbar_width;
                const float scrollbar_y_min = window_rect->position.y + window_top_bar_height;
                const float scrollbar_y_max = window_rect->position.y + window_rect->size.y - window_scrollbar_height - window_resize_control_size.y;
                const float scrollbar_y = remap_clamped(in_window->scroll_amount, 0.0, max_scroll_amount, scrollbar_y_min, scrollbar_y_max);

                const GuiRect scrollbar_draw_rect = {
                    .position =
                        {
                            .x = scrollbar_x,
                            .y = scrollbar_y,
                        },
                    .size =
                        {
                            .x = window_scrollbar_width,
                            .y = window_scrollbar_height,
                        },
                    .z_order = window_rect->z_order,
                };

                GuiRect hit_rect = in_window->is_scrolling ? gui_make_fullscreen_rect(in_context, window_rect->z_order) : scrollbar_draw_rect;
                GuiClickState scrollbar_click_state = gui_make_button(in_context, "", &scrollbar_draw_rect, &hit_rect, &default_button_style, true);
                in_window->is_scrolling = scrollbar_click_state == GUI_HELD;
                if (in_window->is_scrolling)
                {
                    // Compute dragged amount in window-space, and use that to recompute scroll_amount
                    const float new_scrollbar_y = scrollbar_draw_rect.position.y + mouse_delta.y;
                    in_window->scroll_amount = remap_clamped(new_scrollbar_y, scrollbar_y_min, scrollbar_y_max, 0.0, max_scroll_amount);
                }
            }

            // Resize Control (bottom right)
            if (in_window->is_resizable)
            {
                const Vec2 resize_control_position = vec2_sub(vec2_add(window_rect->position, window_rect->size), window_resize_control_size);

                const GuiRect resize_control_rect = {
                    .position = resize_control_position,
                    .size = window_resize_control_size,
                    .z_order = window_rect->z_order,
                };

                in_window->is_resizing = gui_make_button(in_context, "", &resize_control_rect, &resize_control_rect, &default_button_style, !in_window->is_moving) == GUI_HELD;
                if (in_window->is_resizing)
                {
                    window_rect->size = vec2_add(window_rect->size, mouse_delta);
                    window_rect->size.x = MAX(min_window_width, window_rect->size.x);
                    window_rect->size.y = MAX(window_resize_control_size.y, window_rect->size.y);

                    float minimum_window_height = window_top_bar_height + window_resize_control_size.y;
                    window_rect->size.y = MAX(minimum_window_height, window_rect->size.y);
                }
            }
        }

        // TODO: Copy contents of our window to in_context vert/indices arrays
    }
}

// TODO: Fix disappearing scrollbar bug (if you resize y too small)

// TODO: "active window" bring to front
