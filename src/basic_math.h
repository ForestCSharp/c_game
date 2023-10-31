#pragma once

#include "assert.h"

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef CLAMP
#define CLAMP(value, min, max) (MAX(min, MIN(max, value)))
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

float float_fractional(const float input)
{
    float integral;
    return modff(input, &integral);
}
