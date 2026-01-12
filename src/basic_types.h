#pragma once

#include "assert.h"
#include "stdbool.h"
#include "stdint.h"

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

// ---- Swap ---- //
#define SWAP(type, x, y) do { \
    type temp = x;            \
    x = y;                    \
    y = temp;                 \
} while(0)

// ---- Array Size ---- //
#define ARRAY_COUNT(in_array) (sizeof(in_array) / sizeof(in_array[0]))

// ---- Bit Comparisons ---- //
#define BIT_COMPARE(bits, single_bit) ((bits & single_bit) == single_bit)

// ---- Static Block  ---- //
#define static_block(...) { static bool has_run = false; if (!has_run) { has_run = true, __VA_ARGS__ } }

// ---- Alignment Helpers ---- //
#define IS_POWER_OF_TWO(val) (((val) != 0) && (((val) & ((val) - 1)) == 0 ))
#define ALIGN_PTR(ptr, align) \
({ \
	assert(IS_POWER_OF_TWO(align) && "Alignment must be a power of two"); \
	(void *)(((uintptr_t)(ptr) + (align) - 1) & ~((align) - 1)); \
})
#define ALIGN_SIZE(size, align) \
({ \
	assert(IS_POWER_OF_TWO(align) && "Alignment must be a power of two"); \
	(((size) + (align) - 1) & ~((align) - 1)); \
})

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

