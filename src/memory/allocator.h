#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "basic_types.h"

//FCS TODO: Figure out how to override malloc for code you call (LD_PRELOAD on Mac/Linux, something else on Windows)
//FCS TODO: arena_allocator
//FCS TODO: tagged allocators

//FCS TODO: Look at Shadow of the Colossus Talk again. use Macros to insert File and Line information
//FCS TODO: Support for virtual memory allocations without commit (mmap/munmap, VirtualAlloc/VirtualFree)

void* mem_alloc(size_t in_size);
void* mem_alloc_zeroed(size_t in_size);
void* mem_realloc(void* in_ptr, size_t in_size);
void mem_free(void* in_ptr);

// Ext Functions provide file and line information to allocation header
void* mem_alloc_ext(size_t in_size, const char* file, int line);
void* mem_alloc_zeroed_ext(size_t in_size, const char* file, int line);
void* mem_realloc_ext(void* in_ptr, size_t in_size, const char* file, int line);

#define FCS_MEM_ALLOC(size) mem_alloc_ext(size, __FILE__, __LINE__)
#define FCS_MEM_ALLOC_ZEROED(size) mem_alloc_zeroed_ext(size, __FILE__, __LINE__)
#define FCS_MEM_REALLOC(ptr, size) mem_realloc_ext(ptr, size, __FILE__, __LINE__)
#define FCS_MEM_FREE(ptr) mem_free(ptr)

#include "threading/threading.h"

#define MEMORY_LOGGING 1
#if MEMORY_LOGGING
	//FCS TODO: Make Atomic Bool...
	bool g_memory_logging = false;
	AtomicInt64 total_allocated_during_log;
	AtomicInt64 total_freed_during_log;

	#define RECORD_ALLOC(size)\
		atomic_i64_add(&total_allocated_memory, size);\
		if (g_memory_logging)\
		{\
			atomic_i64_add(&total_allocated_during_log, size);\
		}

	#define RECORD_FREE(size)\
		atomic_i64_add(&total_allocated_memory, -size);\
		if (g_memory_logging)\
		{\
			atomic_i64_add(&total_freed_during_log, size);\
		}

	#define ENABLE_MEMORY_LOGGING()\
		g_memory_logging = true;\
		atomic_i64_set(&total_allocated_during_log, 0);\
		atomic_i64_set(&total_freed_during_log, 0);

	#define DISABLE_MEMORY_LOGGING()\
		g_memory_logging = false;

	#define MEMORY_LOG(ptr, ...) \
		if (g_memory_logging)\
		{\
			allocation_log_metadata(ptr);\
			__VA_ARGS__;\
			printf("\n");\
		}
	
	#define MEMORY_LOG_STATS()\
		printf("Memory Log Session: Total Allocated: %i, Total Freed: %i\n",\
			atomic_i64_get(&total_allocated_during_log),\
			atomic_i64_get(&total_freed_during_log)\
		);

#else // MEMORY_LOGGING (disabled)
	#define RECORD_ALLOC(size)\
		atomic_i64_add(&total_allocated_memory, size);
	#define RECORD_FREE(size)\
		atomic_i64_add(&total_allocated_memory, -size);

	#define ENABLE_MEMORY_LOGGING() 
	#define DISABLE_MEMORY_LOGGING()
	#define MEMORY_LOG(ptr, ...)
	#define MEMORY_LOG_STATS()
#endif // MEMORY_LOGGING

#define ALLOCATOR_USE_STD_LIB_FUNCTIONS 1
#if ALLOCATOR_USE_STD_LIB_FUNCTIONS

static AtomicInt64 total_allocated_memory;

#if MEMORY_LOGGING
static AtomicInt64 next_allocation_id;
#endif // MEMORY_LOGGING

typedef struct AllocationHeader
{
	size_t allocation_size;

	#if MEMORY_LOGGING
	bool has_metadata;
	int allocation_id;
	const char* file;
	int line;
	#endif // MEMORY_LOGGING
} AllocationHeader;

const size_t ALLOCATION_HEADER_SIZE = sizeof(AllocationHeader);

i32 get_allocated_memory()
{
	return atomic_i64_get(&total_allocated_memory);
}

void* allocation_header_setup(char* in_ptr, size_t in_allocation_size, int in_allocation_id)
{
	AllocationHeader header = {
		.allocation_size = in_allocation_size,

		#if MEMORY_LOGGING
		.has_metadata = false,
		.allocation_id = in_allocation_id,
		.file = NULL,
		.line = -1,
		#endif // MEMORY_LOGGING
	};
	*(AllocationHeader*) in_ptr = header;
	in_ptr += ALLOCATION_HEADER_SIZE;
	return (void*) in_ptr;
}

AllocationHeader* allocation_get_header(void* in_ptr)
{
	return (AllocationHeader*) in_ptr - 1;	
}

void allocation_header_set_metadata(void* in_ptr, const char* file, int line)
{
	#if MEMORY_LOGGING	
	AllocationHeader* header = allocation_get_header(in_ptr);
	header->has_metadata = true;

	// Reuse existing allocation id if its valid
	header->allocation_id = header->allocation_id > 0 
							? header->allocation_id
							: atomic_i64_add(&next_allocation_id, 1);
	header->file = file;
	header->line = line;
	#endif //MEMORY_LOGGING
}

#if MEMORY_LOGGING	
void allocation_log_metadata(void* in_ptr)
{
	AllocationHeader* header = in_ptr ? allocation_get_header(in_ptr) : NULL;
	if (header && header->has_metadata)
	{
		printf("(id: %i, file: %s: line: %i): ", header->allocation_id, header->file, header->line);
	}
}
#endif //MEMORY_LOGGING

size_t allocation_get_size(void* in_ptr)
{
	return allocation_get_header(in_ptr)->allocation_size + ALLOCATION_HEADER_SIZE;
}

void* mem_alloc(size_t in_size)
{
	size_t actual_size = in_size + ALLOCATION_HEADER_SIZE;

	RECORD_ALLOC(actual_size);

	void* out_ptr = malloc(actual_size);
	out_ptr = allocation_header_setup(out_ptr, in_size, -1);
	return out_ptr;
}

void* mem_alloc_zeroed(size_t in_size)
{
	size_t actual_size = in_size + ALLOCATION_HEADER_SIZE;

	RECORD_ALLOC(actual_size);

	void* out_ptr = calloc(1, actual_size);
	out_ptr = allocation_header_setup(out_ptr, in_size, -1);
	return out_ptr;	
}

void* mem_realloc(void* in_ptr, size_t in_size)
{
	char* actual_ptr = in_ptr;
	int allocation_id = -1;
	if (actual_ptr != NULL)
	{
		size_t old_size = allocation_get_size(in_ptr);

		#if MEMORY_LOGGING
		allocation_id = allocation_get_header(in_ptr)->allocation_id;
		#endif

		RECORD_FREE(old_size);
		actual_ptr -= ALLOCATION_HEADER_SIZE;
	}
	size_t actual_size = in_size + ALLOCATION_HEADER_SIZE;
	RECORD_ALLOC(actual_size);

	void* out_ptr = realloc(actual_ptr, actual_size);
	out_ptr = allocation_header_setup(out_ptr, in_size, allocation_id);
	return out_ptr;
}

void mem_free(void* in_ptr)
{
	if (in_ptr)
	{
		size_t actual_size = allocation_get_size(in_ptr);

		RECORD_FREE(actual_size);
		MEMORY_LOG(in_ptr, printf("mem_free size: %zu", actual_size));

		void* actual_ptr = (void*) ((char*) in_ptr - ALLOCATION_HEADER_SIZE);
		free(actual_ptr);
	}
}

void* mem_alloc_ext(size_t in_size, const char* file, int line)
{
	void* result = mem_alloc(in_size);
	allocation_header_set_metadata(result, file, line);
	MEMORY_LOG(result, printf("mem_alloc size: %zu", allocation_get_size(result)));
	return result;
}

void* mem_alloc_zeroed_ext(size_t in_size, const char* file, int line)
{
	void* result = mem_alloc_zeroed(in_size);
	allocation_header_set_metadata(result, file, line);
	MEMORY_LOG(result, printf("mem_alloc_zeroed size: %zu", allocation_get_size(result)));
	return result;
}

void* mem_realloc_ext(void* in_ptr, size_t in_size, const char* file, int line)
{
	void* result = mem_realloc(in_ptr, in_size);
	allocation_header_set_metadata(result, file, line);
	MEMORY_LOG(result, printf("mem_realloc size: %zu", allocation_get_size(result)));
	return result;
}

#else // ALLOCATOR_USE_STD_LIB_FUNCTIONS

#if defined(_WIN32)
#include "win32/allocator.h"
#endif

#if defined(__APPLE__)
#include "mac/allocator.h"
#endif

#endif // ALLOCATOR_USE_STD_LIB_FUNCTIONS
