#pragma once

#include "stdbool.h"

#include "windows.h"

static const int KEY_ESCAPE = VK_ESCAPE;
static const int KEY_SHIFT = VK_SHIFT;
static const int KEY_SPACE = VK_SPACE;
static const int KEY_LEFT_MOUSE = VK_LBUTTON;
static const int KEY_RIGHT_MOUSE = VK_RBUTTON;
static const int KEY_MIDDLE_MOUSE = VK_MBUTTON;
//TODO: More Special Key Codes (VK_TAB, VK_CTRL, etc.)

bool input_pressed(int key_code) {
    return GetKeyState(key_code) < 0;
}