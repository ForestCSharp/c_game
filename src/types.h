#pragma once

#include "assert.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"

// ---- Primitive Typedefs ---- //
typedef uint64_t u64;
typedef int64_t i64;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint16_t u16;
typedef int16_t i16;

typedef uint8_t u8;
typedef int8_t i8;

typedef float f32;
typedef double f64;

// ---- Array Size ---- //
#define ARRAY_SIZE(in_array) (sizeof(in_array) / sizeof(in_array[0]))

// ---- Bit Comparisons ---- //
#define BIT_COMPARE(bits, single_bit) ((bits & single_bit) == single_bit)

// ---- Static Block  ---- //

#define static_block(...) { static bool has_run = false; if (!has_run) { has_run = true, __VA_ARGS__ } }

// ---- Optional Type ---- //

#define declare_optional_type(type) \
    typedef struct optional_##type  \
    {                               \
        type value;                 \
        bool is_set;                \
    } optional_##type;


#define optional(type) optional_##type

#define optional_init(...) { .value = __VA_ARGS__, .is_set = true,}

#define optional_set(in_opt, in_value) \
    {                                  \
        in_opt.value = in_value;       \
        in_opt.is_set = true;          \
    }
#define optional_is_set(in_opt) in_opt.is_set
#define optional_get(in_opt)    in_opt.value
#define optional_none() {.value = {}, .is_set = false }

declare_optional_type(f32);

// ---- String Type ---- //
typedef struct String
{	
	char* data; // String data (with null-terminator)
	u64 length; // Length (excluding null-terminator)
} String;

declare_optional_type(String);

static const String empty_string = {
	.data = NULL,
	.length = 0,
};

String string_new(const char* in_c_string)
{
	size_t length = strlen(in_c_string);
	char* data = malloc(length + 1);
	memcpy(data, in_c_string, length);
	data[length] = '\0';
	return (String) {
		.data = data,
		.length = length,
	};
}

void string_append(String* in_string, const char* in_c_string_to_append)
{
	const u64 new_length = in_string->length + strlen(in_c_string_to_append);
	assert(new_length > in_string->length);

	in_string->data = realloc(in_string->data, new_length + 1);
	in_string->length = new_length;

	strcat(in_string->data, in_c_string_to_append);
	in_string->data[in_string->length] = '\0';
}

void string_print(String* in_string)
{
	if (in_string->data != NULL && in_string->length > 0)
	{
		printf("%.*s", (i32)in_string->length, in_string->data);
	}
	else
	{
		printf("[EMPTY_STRING]");
	}
}

void string_free(String* in_string)
{
	assert(in_string->data != NULL && in_string->length > 0);
	free(in_string->data);
	*in_string = (String){};
}


static bool read_binary_file(const String* filename, size_t *out_file_size, void **out_data)
{
    FILE *file = fopen(filename->data, "rb");
    if (!file)
	{
        return false;
	}
	
    fseek(file, 0L, SEEK_END);
    const size_t file_size = *out_file_size = ftell(file);
    rewind(file);

    *out_data = calloc(1, file_size + 1);
    if (!*out_data)
    {
        fclose(file);
        return false;
    }

    if (fread(*out_data, 1, file_size, file) != file_size)
    {
        fclose(file);
        free(*out_data);
        return false;
    }

    fclose(file);
    return true;
}

// FCS TODO: replace these with proper tracked allocations, report leaks at end of execution
#define MALLOC(size) malloc(size); printf("malloc: %s line %d\n", __FILE__, __LINE__);
#define CALLOC(num, size) calloc(num, size); printf("calloc: %s line %d\n", __FILE__, __LINE__);
#define REALLOC(ptr, size) realloc(ptr, size); printf("realloc: %s line %d\n", __FILE__, __LINE__);
