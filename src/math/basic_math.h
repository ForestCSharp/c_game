#pragma once

#include "assert.h"
#include "math.h"

#include "types.h"

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef CLAMP
#define CLAMP(value, min, max) (MAX(min, MIN(max, value)))
#endif

#ifndef ABS
#define ABS(value) (value < 0 ? -value : value)
#endif

float lerp(float t, float a, float b)
{
    return (a * (1.0 - t)) + (b * t);
}

float unlerp(float t, float a, float b)
{
    if (a != b)
    {
        return (t - a) / (b - a);
    }
    else
    {
        return 0.f;
    }
}

float remap(float x, float in_range_min, float in_range_max, float out_range_min, float out_range_max)
{
    return lerp(unlerp(x, in_range_min, in_range_max), out_range_min, out_range_max);
}

float remap_clamped(float x, float in_range_min, float in_range_max, float out_range_min, float out_range_max)
{
    return remap(CLAMP(x, in_range_min, in_range_max), in_range_min, in_range_max, out_range_min, out_range_max);
}

static const float PI = 3.14159265358979323846;
static const float DEGREES_TO_RADIANS = PI / 180.0f;
static const float RADIANS_TO_DEGREES = 1.0f / DEGREES_TO_RADIANS;

float degrees_to_radians(const float degrees)
{
    return degrees * DEGREES_TO_RADIANS;
}

float radians_to_degrees(const float radians)
{
    return radians * RADIANS_TO_DEGREES;
}

float f32_fractional(const float input)
{
    float integral;
    return modff(input, &integral);
}

#define ZERO_TOLERANCE 0.0001

bool f32_nearly_zero(const float value)
{
	return ABS(value) <= ZERO_TOLERANCE;
}

bool f32_nearly_equal(const float a, const float b)
{
	return f32_nearly_zero(a - b);
}

f32 rand_f32(f32 lower_bound, f32 upper_bound)
{
	return lower_bound + (f32)(rand()) / ((f32)(RAND_MAX/(upper_bound-lower_bound)));
}

f64 rand_f64(f64 lower_bound, f64 upper_bound)
{
	return lower_bound + (f64)(rand()) / ((f64)(RAND_MAX/(upper_bound-lower_bound)));
}

i32 rand_i32(i32 lower_bound, i32 upper_bound)
{
	return lower_bound + (i32)(rand()) / ((i32)(RAND_MAX/(upper_bound-lower_bound)));
}

i64 rand_i64(i64 lower_bound, i64 upper_bound)
{
	return lower_bound + (i64)(rand()) / ((i64)(RAND_MAX/(upper_bound-lower_bound)));
}
