
// Threading/Sync Functions
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct Thread
{
	pthread_t posix_thread;
} Thread;

typedef struct PThreadPayload
{
	int (*thread_function)(void *);
	void* thread_argument;
} PThreadPayload;

void* pthread_function(void* arg)
{
	PThreadPayload* pthread_payload = (PThreadPayload*) arg;
	pthread_payload->thread_function(pthread_payload->thread_argument);
	free(pthread_payload);
	return NULL;
}

void app_thread_create(app_thread_function_ptr thread_function, void* thread_argument, Thread* out_thread)
{
	PThreadPayload* pthread_payload = malloc(sizeof(PThreadPayload));
	pthread_payload->thread_function = thread_function;
	pthread_payload->thread_argument = thread_argument;
	assert(pthread_create(&out_thread->posix_thread, NULL, pthread_function, (void *)pthread_payload) == 0);
}

void app_thread_join(Thread* in_thread)
{
	assert(pthread_join(in_thread->posix_thread, NULL) == 0);
}

void app_thread_kill(Thread* in_thread)
{
	assert(pthread_cancel(in_thread->posix_thread) == 0);
}

i32 app_get_core_count()
{
	long num_processors = sysconf(_SC_NPROCESSORS_ONLN);
	return (i32) num_processors;
}

typedef struct Mutex
{
	pthread_mutex_t posix_mutex;	
} Mutex;

void app_mutex_create(Mutex* out_mutex)
{
	assert(out_mutex);
	*out_mutex = (Mutex) {};
	assert(pthread_mutex_init(&out_mutex->posix_mutex, NULL) == 0);
}

void app_mutex_destroy(Mutex* in_mutex)
{
	assert(pthread_mutex_destroy(&in_mutex->posix_mutex) == 0);
}

void app_mutex_lock(Mutex* in_mutex)
{
	pthread_mutex_lock(&in_mutex->posix_mutex);
}

void app_mutex_unlock(Mutex* in_mutex)
{
	pthread_mutex_unlock(&in_mutex->posix_mutex);
}

typedef struct Semaphore 
{
	const char* name;
	sem_t* posix_semaphore;
} Semaphore;

void app_semaphore_create(const char* in_name, const i32 in_value, Semaphore* out_semaphore)
{
	assert(out_semaphore);
	const mode_t mode = 0644;
	sem_t* posix_semaphore = sem_open(in_name, O_CREAT, mode, in_value);

	if (posix_semaphore == SEM_FAILED)
	{
		fprintf(stderr, "ERROR: sem_open failed. Code: %d, Description: %s\n", errno, strerror(errno));
	}

	assert(posix_semaphore != SEM_FAILED);

	*out_semaphore = (Semaphore) {
		.name = in_name,
		.posix_semaphore = posix_semaphore,
	};
}

void app_semaphore_destroy(Semaphore* in_semaphore)
{	
	sem_close(in_semaphore->posix_semaphore);
	sem_unlink(in_semaphore->name);
}

void app_semaphore_wait(Semaphore* in_semaphore)
{
	sem_wait(in_semaphore->posix_semaphore);
}

void app_semaphore_post(Semaphore* in_semaphore)
{
	sem_post(in_semaphore->posix_semaphore);	
}

typedef struct AtomicBool
{
	volatile int atomic_int;
} AtomicBool;

void atomic_bool_set(AtomicBool* in_atomic, bool in_new_value)
{
	if (in_new_value)
	{
		__sync_lock_test_and_set(&in_atomic->atomic_int, 1);
	}
	else
	{
		__sync_and_and_fetch(&in_atomic->atomic_int, 0);
	}
}

bool atomic_bool_get(AtomicBool* in_atomic)
{
	return __sync_add_and_fetch(&in_atomic->atomic_int, 0) != 0;
}
