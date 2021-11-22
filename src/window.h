#pragma once

#include "stdbool.h"
#include "windows.h"

typedef struct {
    HINSTANCE hinstance;
    HWND hwnd;
} Window;

Window window_create(const char* name, int width, int height);
bool window_handle_messages(const Window* const window);
void window_get_dimensions(const Window* const window, int* out_width, int* out_height);
void window_get_mouse_pos(const Window* const window, int32_t* out_mouse_x, int32_t* out_mouse_y);