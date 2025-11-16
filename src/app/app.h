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

// Application Functions
Window window_create(const char* name, int width, int height);
bool window_handle_messages(Window* window);
void window_show_mouse_cursor(Window* window, bool in_show_mouse_cursor);
void window_get_position(const Window* const window, int* out_x, int* out_y); //TopLeft position
void window_get_dimensions(const Window* const window, int* out_width, int* out_height);
void window_get_mouse_pos(const Window* const window, i32* out_mouse_x, i32* out_mouse_y);
void window_set_mouse_pos(const Window* const window, i32 in_mouse_x, i32 in_mouse_y);
void window_get_mouse_delta(const Window* const window, i32* out_mouse_delta_x, i32* out_mouse_delta_y);
bool window_input_pressed(const Window* const window, int key_code);

// Threading/Sync Functions
typedef struct Thread Thread;
typedef int (*app_thread_function_ptr)(void *); 
void app_thread_create(app_thread_function_ptr thread_function, void* thread_argument, Thread* out_thread);
void app_thread_join(Thread* in_thread);
void app_thread_kill(Thread* in_thread);
i32 app_get_core_count();

typedef struct Mutex Mutex;
void app_mutex_create(Mutex* out_mutex);
void app_mutex_destroy(Mutex* in_mutex);
void app_mutex_lock(Mutex* in_mutex);
void app_mutex_unlock(Mutex* in_mutex);

typedef struct Semaphore Semaphore;
void app_semaphore_create(const char* in_name, const i32 in_value, Semaphore* out_semaphore);
void app_semaphore_destroy(Semaphore* in_semaphore);
void app_semaphore_wait(Semaphore* in_semaphore);
void app_semaphore_post(Semaphore* in_semaphore);

typedef struct AtomicBool AtomicBool;
void atomic_bool_set(AtomicBool* in_atomic, bool in_new_value);
bool atomic_bool_get(AtomicBool* in_atomic);

#if defined(_WIN32)
#include "win32/app.h"
#endif

#if defined(__APPLE__)
#include "mac/app.h"
#endif

