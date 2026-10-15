#pragma once

#include "memory/allocator.h"
#include "memory/arena.h"

static bool read_binary_file(const char* filename, Arena* optional_arena, size_t *out_file_size, void **out_data)
{
    FILE* file = fopen(filename, "rb");
    if (!file)
	{
        return false;
	}
	
    fseek(file, 0L, SEEK_END);
    const size_t file_size = *out_file_size = ftell(file);
    rewind(file);

    if (optional_arena != NULL)
    {
        *out_data = arena_alloc(optional_arena, file_size);
    }
    else
    {
        *out_data = FCS_MEM_ALLOC_ZEROED(file_size);
    }

    if (!*out_data)
    {
        fclose(file);
        return false;
    }

    if (fread(*out_data, 1, file_size, file) != file_size)
    {
        fclose(file);
        FCS_MEM_FREE(*out_data);
        return false;
    }

    fclose(file);
    return true;
}
