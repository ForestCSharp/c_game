#pragma once

#include "types.h"

typedef struct Window Window;

typedef enum KeyCode
{
    /* 0 - 127 reserved for ASCII */
    KEY_ESCAPE = 128,
    KEY_SHIFT = 129,
    KEY_SPACE = 130,
	KEY_LEFT_CONTROL = 131,
    KEY_LEFT_MOUSE = 132,
    KEY_RIGHT_MOUSE = 133,
    KEY_MIDDLE_MOUSE = 134,
    KEY_MAX_VALUE,
} KeyCode;

Window window_create(const char* name, int width, int height);
bool window_handle_messages(Window* window);
void window_show_mouse_cursor(Window* window, bool in_show_mouse_cursor);
void window_get_position(const Window* const window, int* out_x, int* out_y); //TopLeft position
void window_get_dimensions(const Window* const window, int* out_width, int* out_height);
void window_get_mouse_pos(const Window* const window, i32* out_mouse_x, i32* out_mouse_y);
void window_set_mouse_pos(const Window* const window, i32 in_mouse_x, i32 in_mouse_y);
void window_get_mouse_delta(const Window* const window, i32* out_mouse_delta_x, i32* out_mouse_delta_y);
bool window_input_pressed(const Window* const window, int key_code);

#if defined(_WIN32)
#include "win32/app.h"
#endif

#if defined(__APPLE__)
#include "mac/app.h"
#endif

