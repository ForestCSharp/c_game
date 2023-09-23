#pragma once
#include "stdio.h"

//Win32 API
#include "windows.h"

typedef struct Window {
    HINSTANCE hinstance;
    HWND hwnd;
} Window;

const char g_WindowClassName[] = "c_game";

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {             
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
   }
   return 0;
}

//TODO: Better error handling
Window window_create(const char* name, int width, int height) {
    Window window;

    window.hinstance = (HINSTANCE)GetModuleHandle(NULL);

    //FIXME: use AdjustWindowRect to create client area of width, height

    WNDCLASSEX wc = {
        .cbSize        = sizeof(WNDCLASSEX),
        .style         = 0,
        .lpfnWndProc   = WndProc,
        .cbClsExtra    = 0,
        .cbWndExtra    = 0,
        .hInstance     = window.hinstance,
        .hIcon         = LoadIcon(NULL, IDI_APPLICATION),
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW+1),
        .lpszMenuName  = NULL,
        .lpszClassName = g_WindowClassName,
        .hIconSm       = LoadIcon(NULL, IDI_APPLICATION),
    };

    if(!RegisterClassEx(&wc)) {
        printf("Error Registering Class \n");
        return window;
    }

	DWORD dw_style = WS_OVERLAPPEDWINDOW;
    DWORD dw_ex_style = WS_EX_CLIENTEDGE;
    RECT window_rect = {
        .left = 0,
        .top = 0,
        .right = width,
        .bottom = height,
    };
    
	AdjustWindowRectEx(&window_rect, dw_style, FALSE, dw_ex_style);		

    window.hwnd = CreateWindowEx(
        dw_ex_style,
        g_WindowClassName,
        name,
        dw_style,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        window_rect.right - window_rect.left, //Calc width
        window_rect.bottom - window_rect.top, //Calc height
        NULL, NULL, window.hinstance, NULL
    );

    if (window.hwnd == NULL) {
        printf("Error creating Window \n");
        return window;
    }

    ShowWindow(window.hwnd, SW_SHOW);

    return window;
}

bool window_handle_messages(const Window* const window) {
    MSG msg;
    bool bGotMessage = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
    if (bGotMessage) {
        TranslateMessage(&msg);
		DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
        {
            return false;
        }
    }
    return true;
}

void window_get_dimensions(const Window* const window, int* out_width, int* out_height)
{
    RECT window_rect;
    if (GetClientRect(window->hwnd, &window_rect))
    {
        *out_width = window_rect.right - window_rect.left;
        *out_height = window_rect.bottom - window_rect.top;
    }
}

void window_get_mouse_pos(const Window* const window, i32* out_mouse_x, i32* out_mouse_y) {
    POINT out_point;
    //TODO: Check these two fns succeeded
    GetCursorPos(&out_point);
    ScreenToClient(window->hwnd, &out_point);
    *out_mouse_x = out_point.x;
    *out_mouse_y = out_point.y;
}

//FCS TODO: per-platform keycode translation function, rather than all these constants (see mac/window.h)
static const int KEY_ESCAPE = VK_ESCAPE;
static const int KEY_SHIFT = VK_SHIFT;
static const int KEY_SPACE = VK_SPACE;
static const int KEY_LEFT_MOUSE = VK_LBUTTON;
static const int KEY_RIGHT_MOUSE = VK_RBUTTON;
static const int KEY_MIDDLE_MOUSE = VK_MBUTTON;
//TODO: More Special Key Codes (VK_TAB, VK_CTRL, etc.)

//FCS TODO: Make input per-window
bool input_pressed(int key_code) {
	return GetKeyState(key_code) < 0;
}

