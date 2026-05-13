#pragma once

#include "memory/allocator.h"

#define KiB * (1024ULL)
#define MiB * (1024ULL * KiB)
#define GiB * (1024ULL * MiB)

typedef struct Arena
{
	void* start;
	void* current;

	u64 total_size;
	u64 remaining_size;
	bool allow_growth;

	struct Arena* next;	
} Arena;

typedef struct ArenaDesc
{
	u64 size;
	bool allow_growth;
} ArenaDesc;

static const ArenaDesc default_arena_desc = {
	.size = 4 KiB,
	.allow_growth = true,
};

Arena* arena_create(const ArenaDesc* in_arena_desc)
{
	const u64 header_size = sizeof(Arena);
	const u64 size = header_size + in_arena_desc->size;
	Arena* arena = FCS_MEM_ALLOC_ZEROED(size);
	void* arena_mem_start = (void*) ((u8*) arena + header_size);

	arena->start = arena_mem_start;
	arena->current = arena_mem_start;
	arena->total_size = in_arena_desc->size;
	arena->remaining_size = in_arena_desc->size;
	arena->allow_growth = in_arena_desc->allow_growth;
	arena->next = NULL;

	return arena;
}

void* arena_alloc(Arena* in_arena, const u64 in_size)
{
	if (in_size > in_arena->remaining_size)
	{
		// If growth is disallowed, return NULL
		if (!in_arena->allow_growth)
		{
			return NULL;
		}

		// Growth allowed, but no next arena exists, so create one
		if (!in_arena->next)
		{
			// Make sure the new arena is big enough to hold the requested size, or at least as big as the current arena
			const u64 new_arena_size = in_size > in_arena->total_size ? in_size : in_arena->total_size;

			ArenaDesc next_desc = { 
				.size = new_arena_size,
				.allow_growth = in_arena->allow_growth,
			};
			in_arena->next = arena_create(&next_desc);
		}

		// Attempt to allocate from the next arena
		return arena_alloc(in_arena->next, in_size);
	}

	void* result = in_arena->current;	
	in_arena->current = (void*) ((u8*) in_arena->current + in_size);
	in_arena->remaining_size -= in_size;
	return result;
}

void arena_destroy(Arena* in_arena)
{
	assert(in_arena);

	if (in_arena->next != NULL)
	{
		arena_destroy(in_arena->next);
	}

	FCS_MEM_FREE(in_arena);	
	in_arena = NULL;
}

//FCS TODO: first user: lcp.h (all vecn,matn,matmn functions that create a new value should pass in arena)
//FCS TODO: For that add an ArenaArray 
