#pragma once
#include "stdio.h"

// Win32 API
#include "windows.h"
#include "winuser.h"

// Raw Input Defines
#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

typedef struct Window
{
    HINSTANCE hinstance;
    HWND hwnd;

	bool is_mouse_in_window;
	bool show_mouse_cursor;
	float mouse_delta_x;
	float mouse_delta_y;
} Window;

const char g_WindowClassName[] = "c_game";

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// TODO: Better error handling
Window window_create(const char* name, int width, int height)
{
	Window window;
    window.hinstance = (HINSTANCE) GetModuleHandle(NULL);

    // FIXME: use AdjustWindowRect to create client area of width, height

    WNDCLASSEX wc = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = 0,
        .lpfnWndProc = WndProc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = window.hinstance,
        .hIcon = LoadIcon(NULL, IDI_APPLICATION),
        .hCursor = NULL,//LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH) (COLOR_WINDOW + 1),
        .lpszMenuName = NULL,
        .lpszClassName = g_WindowClassName,
        .hIconSm = LoadIcon(NULL, IDI_APPLICATION),
    };

    if (!RegisterClassEx(&wc))
    {
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
		CW_USEDEFAULT,
		CW_USEDEFAULT,
        window_rect.right - window_rect.left, // Calc width
        window_rect.bottom - window_rect.top, // Calc height
        NULL, NULL, window.hinstance, NULL
	);

    if (window.hwnd == NULL)
    {
        printf("Error creating Window \n");
        return window;
    }

    ShowWindow(window.hwnd, SW_SHOW);
	SetCursor(LoadCursor(NULL, IDC_ARROW));

	RAWINPUTDEVICE raw_input_device;
    raw_input_device.usUsagePage = HID_USAGE_PAGE_GENERIC;
	raw_input_device.usUsage = HID_USAGE_GENERIC_MOUSE;
	raw_input_device.dwFlags = 0; 
	raw_input_device.hwndTarget = window.hwnd;

    assert(RegisterRawInputDevices(&raw_input_device, 1, sizeof(RAWINPUTDEVICE)));

    return window;
}

bool window_handle_messages(Window* window)
{
	// Zero-Out Mouse Delta
	window->mouse_delta_x = 0;
	window->mouse_delta_y = 0;

    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

		switch(msg.message)
		{
			case WM_QUIT:
			{
				return false;
			}
			case WM_MOUSELEAVE:
			{
				window->is_mouse_in_window = false;
				break;
			}
			case WM_MOUSEMOVE:
			{
				if (!window->is_mouse_in_window)
				{
					TRACKMOUSEEVENT tme;
					tme.cbSize = sizeof(tme);
					tme.hwndTrack = window->hwnd;
					tme.dwFlags = TME_LEAVE;
					tme.dwHoverTime = 1;
					TrackMouseEvent(&tme);

					window->is_mouse_in_window = true;
					window_show_mouse_cursor(window, window->show_mouse_cursor);
				}

				break;
			}
			case WM_INPUT:
			{
				if (msg.wParam == RIM_INPUT)
				{
					UINT dwSize = sizeof(RAWINPUT);
					RAWINPUT raw;
					if (GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, &raw, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
					{
						//FCS TODO: REMOVE THIS CHECK.... and printf
						printf("GetRawInputData does not return correct size !\n");
					}

					if (raw.header.dwType == RIM_TYPEMOUSE)
					{
						if ((raw.data.mouse.usFlags & MOUSE_MOVE_RELATIVE) == MOUSE_MOVE_RELATIVE)
						{
							window->mouse_delta_x = raw.data.mouse.lLastX;
							window->mouse_delta_y = raw.data.mouse.lLastY;
						}
					}
				}
				break;
			}
		}
    }
    return true;
}

void window_show_mouse_cursor(Window* window, bool in_show_mouse_cursor)
{
	window->show_mouse_cursor = in_show_mouse_cursor;
	if (window->is_mouse_in_window)
	{
		SetCursor(window->show_mouse_cursor ? LoadCursor(NULL, IDC_ARROW) : NULL);
	}
}

void window_get_position(const Window* const window, int* out_x, int* out_y)
{
	RECT window_rect;
	if (GetClientRect(window->hwnd, &window_rect))
	{
		*out_x = window_rect.left;
		*out_y = window_rect.top;
	}
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

void window_get_mouse_pos(const Window* const window, i32* out_mouse_x, i32* out_mouse_y)
{
    POINT out_point = {};
    // TODO: Check these two fns succeeded
    if (GetCursorPos(&out_point) && ScreenToClient(window->hwnd, &out_point))
	{
		*out_mouse_x = out_point.x;
		*out_mouse_y = out_point.y;
	}
	else
	{
		*out_mouse_x = 0;
		*out_mouse_y = 0;
	}
}

void window_set_mouse_pos(const Window* const window, i32 in_mouse_x, i32 in_mouse_y)
{
	POINT p = {
		.x = in_mouse_x,
		.y = in_mouse_y,
	};
	ClientToScreen(window->hwnd, &p);
	SetCursorPos(p.x, p.y);
}

void window_get_mouse_delta(const Window* const window, float* out_mouse_delta_x, float* out_mouse_delta_y)
{
	*out_mouse_delta_x = window->mouse_delta_x;
	*out_mouse_delta_y = window->mouse_delta_y;
}

int translate_win32_key_code(int key_code)
{
    switch (key_code)
    {
        case KEY_ESCAPE: return VK_ESCAPE;
        case KEY_SHIFT: return VK_SHIFT;
        case KEY_SPACE: return VK_SPACE;
		case KEY_LEFT_CONTROL: return VK_LCONTROL;
        case KEY_LEFT_MOUSE: return VK_LBUTTON;
        case KEY_RIGHT_MOUSE: return VK_RBUTTON;
        case KEY_MIDDLE_MOUSE: return VK_MBUTTON;
        default: return key_code;
    }
}

bool window_input_pressed(const Window* const window, int key_code)
{
    return window->is_mouse_in_window && GetKeyState(translate_win32_key_code(key_code)) < 0;
}
