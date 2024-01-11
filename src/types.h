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

// ---- Optional Type ---- //

#define declare_optional_type(type) \
    typedef struct optional_##type  \
    {                               \
        type value;                 \
        bool is_set;                \
    } optional_##type;


#define optional(type) optional_##type

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
	char* data;
	u64 length;
} String;

declare_optional_type(String);

static const String empty_string = {
	.data = NULL,
	.length = 0,
};

String string_new(const char* in_c_string)
{
	size_t length = strlen(in_c_string);
	char* data = malloc(length);
	memcpy(data, in_c_string, length);
	return (String) {
		.data = data,
		.length = length,
	};
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
