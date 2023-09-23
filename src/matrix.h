#pragma once

#include "types.h"
#define _USE_MATH_DEFINES
#include <math.h>

#include "vec.h"

typedef struct Mat4 {
    float d[4][4];
} Mat4;

const Mat4 mat4_identity = {
    .d = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    },
};

static inline void mat4_print(const Mat4 m) {
    printf("[");
    for (u32 row = 0; row < 4; ++row) {
        if (row > 0) { printf(" "); }
        for (u32 col = 0; col < 4; ++col) {
            printf("%f", m.d[row][col]);
            if (row < 3 || col < 3) { printf(", "); }
        }
        if (row < 3) { printf("\n"); }
    }
    printf("]\n");
}

Mat4 mat4_mult_mat4(const Mat4 a, const Mat4 b) {
    Mat4 result = {0};

    for (u32 row = 0; row < 4; ++row) {
        for (u32 col = 0; col < 4; ++col) {
            for (u32 i = 0; i < 4; ++i) {
                result.d[row][col] += a.d[row][i] * b.d[i][col];
            }
        }
    }

    return result;
}

Vec4 mat4_mult_vec4(const Mat4 m, const Vec4 v) {
    return (Vec4) {
        .x = m.d[0][0] * v.x + m.d[0][1] * v.y + m.d[0][2] * v.z + m.d[0][3] * v.w,
        .y = m.d[1][0] * v.x + m.d[1][1] * v.y + m.d[1][2] * v.z + m.d[1][3] * v.w,
        .z = m.d[2][0] * v.x + m.d[2][1] * v.y + m.d[2][2] * v.z + m.d[2][3] * v.w,
        .w = m.d[3][0] * v.x + m.d[3][1] * v.y + m.d[3][2] * v.z + m.d[3][3] * v.w,
    };
}

Mat4 mat4_translation(const Vec3 position) {
    Mat4 result = mat4_identity;
    result.d[3][0] = position.x;
    result.d[3][1] = position.y;
    result.d[3][2] = position.z;
    return result;
}

Mat4 mat4_scale(const Vec3 scale) {
    Mat4 result = mat4_identity;
    result.d[0][0] = scale.x;
    result.d[1][1] = scale.y;
    result.d[2][2] = scale.z;
    return result;
}

Mat4 mat4_look_at(const Vec3 from, const Vec3 to, const Vec3 in_up) {
    Vec3 forward = vec3_sub(to, from);
    forward = vec3_normalize(forward);
    
    Vec3 right = vec3_cross(forward, in_up);
    right = vec3_normalize(right);

    Vec3 up = vec3_cross(right, forward);

    float rde = -1.0 * vec3_dot(right, from);
    float ude = -1.0 * vec3_dot(up, from);
    float fde = -1.0 * vec3_dot(forward, from);

    return (Mat4) {
        .d = {
            right.x,    up.x,    forward.x,    0,
            right.y,       up.y,       forward.y,       0,
            right.z,  up.z,  forward.z,  0,
            rde,    ude,    fde,    1,
        }
    };
}

Mat4 mat4_perspective(const float fov, const float aspect_ratio, const float near, const float far) {
    float D2R = M_PI / 180.0f;
    float y_scale = 1.0 / tan(D2R * fov / 2);
    float x_scale = y_scale / aspect_ratio;
    float near_minus_far = near - far;

    return (Mat4) {
        .d = {  x_scale, 0, 0, 0,
                0, y_scale * -1.0, 0, 0,
                0, 0, (far + near) / near_minus_far, -1,
                0, 0, 2*far*near / near_minus_far, 0
        },
    };
}

Mat4 mat4_angle_axis(float x, float y, float z, float theta) {
    const float size_squared = x * x + y * y + z * z;
    if (size_squared - 1.0f > 0.00001f) {
        const float size = sqrtf(size_squared);
        x /= size;
        y /= size;
        z /= size;
    }

    float cos = cosf(theta);
    float sin = sinf(theta);

    return (Mat4) {
       .d = {   cos + x * x * (1 - cos), x * y * (1 - cos) - z * sin, x * z * (1 - cos) + y * sin, 0.0f,
                y * x * (1 - cos) + z * sin, cos + y * y * (1 - cos), y * z * (1 - cos) - x * sin, 0.0f,
                z * x * (1 - cos) - y * sin, z * y * (1 - cos) + x * sin, cos + z * z * (1 - cos), 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
       },
    };
}
