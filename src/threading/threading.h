#pragma once

#include "basic_types.h"

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

typedef struct AtomicInt32 AtomicInt32;
i32 atomic_i32_set(AtomicInt32* in_atomic, i32 in_new_value);
i32 atomic_i32_add(AtomicInt32* in_atomic, i32 in_value_to_add);
i32 atomic_i32_get(AtomicInt32* in_atomic);

typedef struct AtomicInt64 AtomicInt64;
i64 atomic_i64_set(AtomicInt64* in_atomic, i64 in_new_value);
i64 atomic_i64_add(AtomicInt64* in_atomic, i64 in_value_to_add);
i64 atomic_i64_get(AtomicInt64* in_atomic);

typedef struct AtomicBool AtomicBool;
void atomic_bool_set(AtomicBool* in_atomic, bool in_new_value);
bool atomic_bool_get(AtomicBool* in_atomic);

#if defined(_WIN32)
#include "win32/threading.h"
#endif

#if defined(__APPLE__)
#include "mac/threading.h"
#endif
