#pragma once

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

declare_optional_type(f32);
