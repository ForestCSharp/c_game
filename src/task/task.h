#include "app/app.h"
#include "stretchy_buffer.h"
#include "memory/allocator.h"

typedef void (*task_function_ptr)(void *); 

typedef struct TaskDesc
{	
	task_function_ptr task_function;	
	void* argument;
} TaskDesc;

typedef struct Task
{
	TaskDesc desc;
	AtomicBool is_complete;
} Task;

typedef struct TaskSystem
{
	// Should not be accessed from task_thread_fn
	sbuffer(Thread) threads;

	// task_thread_fn will pull from these pending tasks
	sbuffer(Task*) pending_tasks;

	// Represents current pending_tasks count
	Semaphore pending_tasks_semaphore;

	// Mutex for accessing pending_tasks
	Mutex pending_tasks_mutex;
} TaskSystem;

int task_thread_fn(void* in_argument)
{
	TaskSystem* task_system = (TaskSystem*) in_argument;

	while (true)
	{
		// use semaphore to wait on pending tasks to avoid busy-waiting
		app_semaphore_wait(&task_system->pending_tasks_semaphore);
		// Lock pending_tasks_mutex so we can access pending_tasks
		app_mutex_lock(&task_system->pending_tasks_mutex);
		// Grab next task by value and execute it
		Task* task = task_system->pending_tasks[0];	
		// Remove Task from pending tasks
		sb_del(task_system->pending_tasks, 0);
		// Unlock pending_tasks_mutex as quickly as we can
		app_mutex_unlock(&task_system->pending_tasks_mutex);
		// Execute the task function after releasing the lock
		task->desc.task_function(task->desc.argument);
		atomic_bool_set(&task->is_complete, true);
	}

	return 0;
}

void task_system_init(TaskSystem* out_task_system)
{
	assert(out_task_system);

	const i32 num_processors = app_get_core_count();
	printf("Num Processors: %i\n", num_processors);

	// Main Thread + Task Threads
	const i32 num_task_processors = num_processors - 1;

	sbuffer(Thread) threads = NULL;
	for (i32 thread_idx = 0; thread_idx < num_task_processors; ++thread_idx)
	{
		Thread new_thread;
		sb_push(threads, new_thread);	
	}

	Semaphore pending_tasks_semaphore;
	app_semaphore_create("Semaphore: Available Tasks", 0, &pending_tasks_semaphore);

	Mutex pending_tasks_mutex;
	app_mutex_create(&pending_tasks_mutex);

	*out_task_system = (TaskSystem) {
		.threads = threads,
		.pending_tasks = NULL,
		.pending_tasks_semaphore = pending_tasks_semaphore,
		.pending_tasks_mutex = pending_tasks_mutex,
	};

	// Spin up threads after allocating all of our data
	for (i32 thread_idx = 0; thread_idx < sb_count(out_task_system->threads); ++thread_idx)
	{
		app_thread_create(task_thread_fn, out_task_system, &out_task_system->threads[thread_idx]);
	}
}

void task_system_shutdown(TaskSystem* in_task_system)
{
	assert(in_task_system);

	for (i32 thread_idx = 0; thread_idx < sb_count(in_task_system->threads); ++thread_idx)
	{
		app_thread_kill(&in_task_system->threads[thread_idx]);
	}
	sb_free(in_task_system->threads);

	app_semaphore_destroy(&in_task_system->pending_tasks_semaphore);
	app_mutex_destroy(&in_task_system->pending_tasks_mutex);

	*in_task_system = (TaskSystem) {};
}

Task* task_system_add_task(TaskSystem* in_task_system, TaskDesc* in_task_desc)
{
	Task* new_task = mem_alloc(sizeof(Task));

	*new_task = (Task) {
		.desc = *in_task_desc,
	};

	// Add new task to pending tasks array
	app_mutex_lock(&in_task_system->pending_tasks_mutex);
	sb_push(in_task_system->pending_tasks, new_task);
	app_mutex_unlock(&in_task_system->pending_tasks_mutex);
	app_semaphore_post(&in_task_system->pending_tasks_semaphore);

	return new_task;
}

// Takes ownership of in_tasks. Frees tasks as they complete and frees in_tasks
void task_system_wait_tasks(TaskSystem* in_task_system, sbuffer(Task*) in_tasks)
{
	// Make sure all tasks have finished
	while(sb_count(in_tasks) > 0)
	{
		Task* task = sb_last(in_tasks);
		if (atomic_bool_get(&task->is_complete))
		{
			mem_free(task);
			sb_del(in_tasks, sb_count(in_tasks) - 1);	
		}
	}
}

i32 task_system_num_threads(TaskSystem* in_task_system)
{
	return sb_count(in_task_system->threads);
}
