
// Threading/Sync Functions
#include <errno.h>
#include <string.h>
#include <windows.h>
#include "memory/allocator.h"

typedef struct Thread
{
	HANDLE win_thread;
	DWORD win_thread_id;
} Thread;

typedef struct WinThreadPayload
{
	int (*thread_function)(LPVOID);
	LPVOID thread_argument;
} WinThreadPayload;

DWORD win_thread_function(LPVOID arg)
{
	WinThreadPayload* winthread_payload = (WinThreadPayload*) arg;
	winthread_payload->thread_function(winthread_payload->thread_argument);
	FCS_MEM_FREE(winthread_payload);
	return 0;
}

void app_thread_create(app_thread_function_ptr thread_function, void* thread_argument, Thread* out_thread)
{
	WinThreadPayload* winthread_payload = FCS_MEM_ALLOC(sizeof(WinThreadPayload));
	winthread_payload->thread_function = thread_function;
	winthread_payload->thread_argument = thread_argument;

	out_thread->win_thread = CreateThread( 
		NULL,						// default security attributes
		0,							// use default stack size  
		win_thread_function,		// thread function name
		(void*) winthread_payload,	// argument to thread function 
		0,							// use default creation flags 
		&out_thread->win_thread_id	// returns the thread identifier 
	);
}

void app_thread_join(Thread* in_thread)
{
	DWORD wait_result = WaitForSingleObject(in_thread->win_thread, INFINITE);
	if (wait_result == WAIT_OBJECT_0)
	{
		// The thread has finished successfully.
		CloseHandle(in_thread->win_thread); 
	}
}

void app_thread_kill(Thread* in_thread)
{
	TerminateThread(in_thread->win_thread, 0);
}

i32 app_get_core_count()
{
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	i32 num_processors = system_info.dwNumberOfProcessors;
	return num_processors;
}

typedef struct Mutex
{
	HANDLE win_mutex;	
} Mutex;

void app_mutex_create(Mutex* out_mutex)
{
	assert(out_mutex);
	*out_mutex = (Mutex) {};
	out_mutex->win_mutex = CreateMutex(NULL, FALSE, NULL);
	assert(out_mutex->win_mutex != NULL);
}

void app_mutex_destroy(Mutex* in_mutex)
{
    assert(CloseHandle(in_mutex->win_mutex) != 0);
}

void app_mutex_lock(Mutex* in_mutex)
{
    WaitForSingleObject(in_mutex->win_mutex, INFINITE);
}

void app_mutex_unlock(Mutex* in_mutex)
{
    assert(ReleaseMutex(in_mutex->win_mutex) != 0);
}

typedef struct Semaphore 
{
    const char* name;
    HANDLE win_semaphore;
} Semaphore;

void app_semaphore_create(const char* in_name, const i32 in_value, Semaphore* out_semaphore)
{
    assert(out_semaphore);

    const i32 max_count = 32767; 
    HANDLE win_semaphore = CreateSemaphoreA(NULL, in_value, max_count, in_name);

    assert(win_semaphore != NULL);

    *out_semaphore = (Semaphore) {
        .name = in_name,
        .win_semaphore = win_semaphore,
    };
}

void app_semaphore_destroy(Semaphore* in_semaphore)
{   
    assert(CloseHandle(in_semaphore->win_semaphore) != 0);
}

void app_semaphore_wait(Semaphore* in_semaphore)
{
    // Decrements the semaphore count; blocks if count is 0.
    WaitForSingleObject(in_semaphore->win_semaphore, INFINITE);
}

void app_semaphore_post(Semaphore* in_semaphore)
{
    // Increments the count by 1. 
    assert(ReleaseSemaphore(in_semaphore->win_semaphore, 1, NULL) != 0);
}

#include "threading/clang/clang_atomics.h"
