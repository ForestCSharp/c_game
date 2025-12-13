#pragma once

#include "stddef.h"

//FCS TODO: Replace all occurrences of malloc/calloc/realloc/free in files
//FCS TODO: Figure out how to override malloc for code you call (LD_PRELOAD on Mac/Linux, something else on Windows)
//FCS TODO: arena_allocator

//FCS TODO: Look at Shadow of the Colossus Talk again. use Macros?
//FCS TODO: Support for virtual memory allocations without commit
//FCS TODO: Platform specific allocators that use mmap and VirtualAlloc

void* mem_alloc(size_t in_size);
void* mem_alloc_zeroed(size_t in_size);
void* mem_realloc(void* in_ptr, size_t in_new_size);
void mem_free(void* in_ptr);

#define ALLOCATOR_USE_STD_LIB_FUNCTIONS 1

#if ALLOCATOR_USE_STD_LIB_FUNCTIONS

void* mem_alloc(size_t in_size)
{
	return malloc(in_size);
}

void* mem_alloc_zeroed(size_t in_size)
{
	return calloc(1, in_size);
}

void* mem_realloc(void* in_ptr, size_t in_new_size)
{
	return realloc(in_ptr, in_new_size);
}

void mem_free(void* in_ptr)
{
	free(in_ptr);
}

#else

#if defined(_WIN32)
#include "win32/allocator.h"
#endif

#if defined(__APPLE__)
#include "mac/allocator.h"
#endif

#endif // ALLOCATOR_USE_STD_LIB_FUNCTIONS
