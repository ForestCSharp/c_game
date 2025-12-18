#pragma once

#include <string.h>

#include "basic_types.h"
#include "memory/allocator.h"

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
	char* data = FCS_MEM_ALLOC(length + 1);
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

	in_string->data = FCS_MEM_REALLOC(in_string->data, new_length + 1);
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
	FCS_MEM_FREE(in_string->data);
	*in_string = (String){};
}
