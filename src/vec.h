#pragma once
#include <math.h>
#include <stdio.h>

typedef struct Vec3 {
    float x;
    float y;
    float z;
} Vec3;

const Vec3 vec3_zero = {.x=0, .y=0, .z=0};

void vec3_print(const Vec3 vec) {
    printf("[%f %f %f] ", vec.x, vec.y, vec.z);
}

Vec3 vec3_new(float x, float y, float z) {
    Vec3 v = {
        .x = x,
        .y = y,
        .z = z
    };
    return v;
}

Vec3 vec3_negate(const Vec3 v) {
    return (Vec3) {
        .x = -v.x,
        .y = -v.y,
        .z = -v.z,
    };
}

Vec3 vec3_scale(const Vec3 v, float a) { 
    return (Vec3) {
        .x = v.x * a,
        .y = v.y * a,
        .z = v.z * a,
    };
}

Vec3 vec3_add(const Vec3 a, const Vec3 b) {
    Vec3 result = {
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
    };
    return result;
}

Vec3 vec3_sub(const Vec3 a, const Vec3 b) {
    return vec3_add(a, vec3_negate(b));
}

float vec3_dot(const Vec3 a, const Vec3 b) {
    float result = a.x * b.x
                 + a.y * b.y
                 + a.z * b.z;
    return result;
}

Vec3 vec3_cross(const Vec3 a, const Vec3 b) {
    Vec3 result = {
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x,
    };
    return result;
}

float vec3_length_squared(const Vec3 v) { 
    return v.x * v.x + v.y * v.y + v.z * v.z; 
}

float vec3_length(const Vec3 v) { 
    return sqrt(vec3_length_squared(v)); 
}

Vec3 vec3_normalize(const Vec3 v) { 
    float length = vec3_length(v);
    return (Vec3) {
        .x = v.x / length,
        .y = v.y / length,
        .z = v.z / length,
    }; 
}

Vec3 vec3_projection(const Vec3 v, const Vec3 dir) {
    float dir_length_squared = vec3_length_squared(dir);
    return vec3_scale(dir, vec3_dot(v, dir) / dir_length_squared);
}

Vec3 vec3_plane_projection(const Vec3 v, const Vec3 plane_normal) {
    return vec3_sub(v, vec3_projection(v, plane_normal));
}

typedef struct Vec4 {
    float x;
    float y;
    float z;
    float w;
} Vec4;

const Vec4 vec4_zero = {.x=0, .y=0, .z=0, .w=0};

void vec4_print(const Vec4 vec) {
    printf("[%f %f %f %f] \n", vec.x, vec.y, vec.z, vec.w);
}

Vec4 vec4_new(float x, float y, float z, float w) {
    return (Vec4) {
        .x = x,
        .y = y,
        .z = z,
        .w = w
    };
}

Vec4 vec4_negate(const Vec4 v) {
    return (Vec4) {
        .x = -v.x,
        .y = -v.y,
        .z = -v.z,
        .w = -v.w,
    };
}

Vec4 vec4_add(const Vec4 a, const Vec4 b) {
    return (Vec4) {
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
        .w = a.w + b.w,
    };
}

Vec4 vec4_sub(const Vec4 a, const Vec4 b) {
    return vec4_add(a, vec4_negate(b));
}

float vec4_dot(const Vec4 a, const Vec4 b) {
    float result = a.x * b.x
                 + a.y * b.y
                 + a.z * b.z
                 + a.w * b.w;
    return result;
}

Vec3 vec4_to_vec3(const Vec4 v) {
    return (Vec3) {
        .x = v.x,
        .y = v.y,
        .z = v.z,
    };
}

Vec4 vec3_to_vec4(const Vec3 v, const float w) {
    return (Vec4) {
        .x = v.x,
        .y = v.y,
        .z = v.z,
        .w = w,
    };
}