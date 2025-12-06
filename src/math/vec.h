#pragma once
#include <math.h>
#include <stdio.h>
#include "math/basic_math.h"

typedef struct Vec2
{
    float x;
    float y;
} Vec2;

const Vec2 vec2_zero = {.x = 0, .y = 0};

void vec2_print(const Vec2 vec)
{
    printf("[%f %f] ", vec.x, vec.y);
}

Vec2 vec2_new(float x, float y)
{
    Vec2 v = {
        .x = x,
        .y = y,
    };
    return v;
}

Vec2 vec2_negate(const Vec2 v)
{
    return (Vec2){
        .x = -v.x,
        .y = -v.y,
    };
}

Vec2 vec2_scale(const Vec2 v, float a)
{
    return (Vec2){
        .x = v.x * a,
        .y = v.y * a,
    };
}

Vec2 vec2_add(const Vec2 a, const Vec2 b)
{
    Vec2 result = {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
    return result;
}

Vec2 vec2_sub(const Vec2 a, const Vec2 b)
{
    return vec2_add(a, vec2_negate(b));
}

float vec2_length_squared(const Vec2 v)
{
    return v.x * v.x + v.y * v.y;
}

float vec2_length(const Vec2 v)
{
    return sqrt(vec2_length_squared(v));
}

Vec2 vec2_normalize(const Vec2 v)
{
    float length = vec2_length(v);
    return (Vec2){
        .x = v.x / length,
        .y = v.y / length,
    };
}

Vec2 vec2_lerp(const float t, const Vec2 a, const Vec2 b)
{
    return vec2_add(vec2_scale(a, 1.0f - t), vec2_scale(b, t));
}

Vec2 vec2_rotate(const Vec2 v, const float radians)
{
    return (Vec2){
        .x = v.x * cos(radians) - v.y * sin(radians),
        .y = v.x * sin(radians) - v.y * cos(radians),
    };
}

bool vec2_nearly_equal(const Vec2 a, const Vec2 b)
{
	return 	f32_nearly_equal(a.x, b.x)
		&&	f32_nearly_equal(a.y, b.y);
}

typedef struct Vec3
{
    float x;
    float y;
    float z;
} Vec3;

declare_optional_type(Vec3);

static const Vec3 vec3_zero = {.x = 0, .y = 0, .z = 0};
static const Vec3 vec3_one = {.x = 1, .y = 1, .z = 1};

void vec3_print(const Vec3 vec)
{
    printf("[%f %f %f] ", vec.x, vec.y, vec.z);
}

Vec3 vec3_new(float x, float y, float z)
{
    Vec3 v = {.x = x, .y = y, .z = z};
    return v;
}

Vec3 vec3_negate(const Vec3 v)
{
    return (Vec3){
        .x = -v.x,
        .y = -v.y,
        .z = -v.z,
    };
}

Vec3 vec3_scale(const Vec3 v, float a)
{
    return (Vec3){
        .x = v.x * a,
        .y = v.y * a,
        .z = v.z * a,
    };
}

Vec3 vec3_add(const Vec3 a, const Vec3 b)
{
    Vec3 result = {
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
    };
    return result;
}

Vec3 vec3_sub(const Vec3 a, const Vec3 b)
{
    return vec3_add(a, vec3_negate(b));
}

Vec3 vec3_mul_componentwise(const Vec3 a, const Vec3 b)
{
	return vec3_new(a.x * b.x, a.y * b.y, a.z * b.z);
}

float vec3_dot(const Vec3 a, const Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vec3_cross(const Vec3 a, const Vec3 b)
{
    Vec3 result = {
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x,
    };
    return result;
}

float vec3_length_squared(const Vec3 v)
{
    return vec3_dot(v, v);
}

float vec3_length(const Vec3 v)
{
    return sqrt(vec3_length_squared(v));
}

Vec3 vec3_normalize(const Vec3 v)
{
    float length = vec3_length(v);
	if (length == 0.f)
	{
		return v;
	}

    return (Vec3){
        .x = v.x / length,
        .y = v.y / length,
        .z = v.z / length,
    };
}

Vec3 vec3_lerp(const float t, const Vec3 a, const Vec3 b)
{
    return vec3_add(vec3_scale(a, 1.0f - t), vec3_scale(b, t));
}

Vec3 vec3_projection(const Vec3 v, const Vec3 dir)
{
    float dir_length_squared = vec3_length_squared(dir);
    return vec3_scale(dir, vec3_dot(v, dir) / dir_length_squared);
}

Vec3 vec3_plane_projection(const Vec3 v, const Vec3 plane_normal)
{
    return vec3_sub(v, vec3_projection(v, plane_normal));
}

bool vec3_nearly_equal(const Vec3 a, const Vec3 b)
{
		return 	f32_nearly_equal(a.x, b.x)
			&&	f32_nearly_equal(a.y, b.y)
			&&	f32_nearly_equal(a.z, b.z);
}

typedef struct Vec4
{
    float x;
    float y;
    float z;
    float w;
} Vec4;

declare_optional_type(Vec4);

const Vec4 vec4_zero = {.x = 0, .y = 0, .z = 0, .w = 0};

void vec4_print(const Vec4 vec)
{
    printf("[%f %f %f %f] \n", vec.x, vec.y, vec.z, vec.w);
}

Vec4 vec4_new(float x, float y, float z, float w)
{
    return (Vec4){.x = x, .y = y, .z = z, .w = w};
}

Vec4 vec4_from_vec3(const Vec3 v, const float w)
{
    return (Vec4){
        .x = v.x,
        .y = v.y,
        .z = v.z,
        .w = w,
    };
}

Vec4 vec4_negate(const Vec4 v)
{
    return (Vec4){
        .x = -v.x,
        .y = -v.y,
        .z = -v.z,
        .w = -v.w,
    };
}

Vec4 vec4_scale(const Vec4 v, float a)
{
    return (Vec4){
        .x = v.x * a,
        .y = v.y * a,
        .z = v.z * a,
        .w = v.w * a,
    };
}

Vec4 vec4_add(const Vec4 a, const Vec4 b)
{
    return (Vec4){
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
        .w = a.w + b.w,
    };
}

Vec4 vec4_sub(const Vec4 a, const Vec4 b)
{
    return vec4_add(a, vec4_negate(b));
}

float vec4_dot(const Vec4 a, const Vec4 b)
{
    float result = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    return result;
}

Vec4 vec4_lerp(const float t, const Vec4 a, const Vec4 b)
{
    return vec4_add(vec4_scale(a, 1.0f - t), vec4_scale(b, t));
}

Vec3 vec4_xyz(const Vec4 v)
{
    return (Vec3){
        .x = v.x,
        .y = v.y,
        .z = v.z,
    };
}

bool vec4_nearly_equal(const Vec4 a, const Vec4 b)
{
	return  f32_nearly_equal(a.x, b.x)
		&&	f32_nearly_equal(a.y, b.y)
		&&	f32_nearly_equal(a.z, b.z)
		&&	f32_nearly_equal(a.w, b.w);
}

// Floating point LHS

Vec2 float_div_vec2(float a, const Vec2 v)
{
    return (Vec2){
        .x = a / v.x,
        .y = a / v.y,
    };
}

Vec3 float_div_vec3(float a, const Vec3 v)
{
    return (Vec3){
        .x = a / v.x,
        .y = a / v.y,
        .z = a / v.z,
    };
}

Vec4 float_div_vec4(float a, const Vec4 v)
{
    return (Vec4){
        .x = a / v.x,
        .y = a / v.y,
        .z = a / v.z,
        .w = a / v.w,
    };
}
