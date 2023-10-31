#pragma once

#include "../types.h"

typedef struct Window Window;

Window window_create(const char* name, int width, int height);
bool window_handle_messages(const Window* const window);
void window_get_dimensions(const Window* const window, int* out_width, int* out_height);
void window_get_mouse_pos(const Window* const window, i32* out_mouse_x, i32* out_mouse_y);

typedef enum KeyCode
{
    /* 0 - 127 reserved for ASCII */
    KEY_ESCAPE = 128,
    KEY_SHIFT = 129,
    KEY_SPACE = 130,
    KEY_LEFT_MOUSE = 131,
    KEY_RIGHT_MOUSE = 132,
    KEY_MIDDLE_MOUSE = 133,
    KEY_MAX_VALUE,
} KeyCode;

bool input_pressed(int key_code);

#if defined(_WIN32)
#include "win32/window.h"
#endif

#if defined(__APPLE__)
#include "mac/window.h"
#endif
