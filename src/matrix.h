#pragma once

#include "assert.h"
#include "basic_math.h"
#include "types.h"
#define _USE_MATH_DEFINES
#include "vec.h"
#include <math.h>

typedef struct Mat4
{
	union
	{
		Vec4 columns[4];
    	float d[4][4];
	};
} Mat4;

declare_optional_type(Mat4);

const Mat4 mat4_identity = {
    .d = {	
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0, 
		0, 0, 0, 1
	},
};

static inline void mat4_print(const Mat4 m)
{
    printf("[");
    for (u32 row = 0; row < 4; ++row)
    {
        if (row > 0)
        {
            printf(" ");
        }
        for (u32 col = 0; col < 4; ++col)
        {
            printf("%f", m.d[row][col]);
            if (row < 3 || col < 3)
            {
                printf(", ");
            }
        }
        if (row < 3)
        {
            printf("\n");
        }
    }
    printf("]\n");
}

Mat4 mat4_mul_mat4(const Mat4 a, const Mat4 b)
{
    Mat4 result = {0};

    for (i32 row = 0; row < 4; ++row)
    {
        for (i32 col = 0; col < 4; ++col)
        {
            for (i32 i = 0; i < 4; ++i)
            {
                result.d[row][col] += a.d[row][i] * b.d[i][col];
            }
        }
    }

    return result;
}

Vec4 mat4_mul_vec4(const Mat4 m, const Vec4 v)
{
    return (Vec4){
        .x = m.d[0][0] * v.x + m.d[0][1] * v.y + m.d[0][2] * v.z + m.d[0][3] * v.w,
        .y = m.d[1][0] * v.x + m.d[1][1] * v.y + m.d[1][2] * v.z + m.d[1][3] * v.w,
        .z = m.d[2][0] * v.x + m.d[2][1] * v.y + m.d[2][2] * v.z + m.d[2][3] * v.w,
        .w = m.d[3][0] * v.x + m.d[3][1] * v.y + m.d[3][2] * v.z + m.d[3][3] * v.w,
    };
}

Mat4 mat4_mul_f32(const Mat4 m, const float f)
{
	Mat4 result = m;
	for (i32 row = 0; row < 4; ++row)
    {
        for (i32 col = 0; col < 4; ++col)
        {
	 		result.d[row][col] *= f; 
		}
	}
	return result;
}

Mat4 mat4_translation(const Vec3 position)
{
    Mat4 result = mat4_identity;
    result.d[3][0] = position.x;
    result.d[3][1] = position.y;
    result.d[3][2] = position.z;
    return result;
}

Mat4 mat4_scale(const Vec3 scale)
{
    Mat4 result = mat4_identity;
    result.d[0][0] = scale.x;
    result.d[1][1] = scale.y;
    result.d[2][2] = scale.z;
    return result;
}

Mat4 mat4_look_at(const Vec3 from, const Vec3 to, const Vec3 in_up)
{
    Vec3 forward = vec3_sub(to, from);
    forward = vec3_normalize(forward);

    Vec3 right = vec3_cross(forward, in_up);
    right = vec3_normalize(right);

    Vec3 up = vec3_cross(right, forward);

    float rde = -1.0 * vec3_dot(right, from);
    float ude = -1.0 * vec3_dot(up, from);
    float fde = -1.0 * vec3_dot(forward, from);

    return (Mat4){.d = {
                      right.x, up.x, forward.x, 0,
                      right.y, up.y, forward.y, 0,
                      right.z, up.z, forward.z, 0,
                      rde, ude, fde,1,
                  }};
}

Mat4 mat4_perspective(const float fov, const float aspect_ratio, const float near, const float far)
{
    float D2R = M_PI / 180.0f;
    float y_scale = 1.0 / tan(D2R * fov / 2);
    float x_scale = y_scale / aspect_ratio;
    float near_minus_far = near - far;

    return (Mat4){
        .d = {x_scale, 0, 0, 0, 0, y_scale * -1.0, 0, 0, 0, 0, (far + near) / near_minus_far, -1, 0, 0, 2 * far * near / near_minus_far, 0},
    };
}

Mat4 mat4_angle_axis(float x, float y, float z, float theta)
{
    const float size_squared = x * x + y * y + z * z;
    if (size_squared - 1.0f > 0.00001f)
    {
        const float size = sqrtf(size_squared);
        x /= size;
        y /= size;
        z /= size;
    }

    float cos = cosf(theta);
    float sin = sinf(theta);

    return (Mat4){
        .d =
            {
                cos + x * x * (1 - cos),
                x * y * (1 - cos) - z * sin,
                x * z * (1 - cos) + y * sin,
                0.0f,
                y * x * (1 - cos) + z * sin,
                cos + y * y * (1 - cos),
                y * z * (1 - cos) - x * sin,
                0.0f,
                z * x * (1 - cos) - y * sin,
                z * y * (1 - cos) + x * sin,
                cos + z * z * (1 - cos),
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
            },
    };
}

bool InverseMat44(const float* m, float* invOut);

optional(Mat4) mat4_inverse(Mat4 in_mat)
{
	optional(Mat4) out_matrix = {};

	Mat4 tmp = {};	
	float* m = (float*) in_mat.d;
	float* inv = (float*) tmp.d;

	inv[0]  =  m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
	inv[4]  = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
	inv[8]  =  m[4] * m[9]  * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
	inv[12] = -m[4] * m[9]  * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
	inv[1]  = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
	inv[5]  =  m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
	inv[9]  = -m[0] * m[9]  * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
	inv[13] =  m[0] * m[9]  * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
	inv[2]  =  m[1] * m[6]  * m[15] - m[1] * m[7]  * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7]  - m[13] * m[3] * m[6];
	inv[6]  = -m[0] * m[6]  * m[15] + m[0] * m[7]  * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7]  + m[12] * m[3] * m[6];
	inv[10] =  m[0] * m[5]  * m[15] - m[0] * m[7]  * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7]  - m[12] * m[3] * m[5];
	inv[14] = -m[0] * m[5]  * m[14] + m[0] * m[6]  * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6]  + m[12] * m[2] * m[5];
	inv[3]  = -m[1] * m[6]  * m[11] + m[1] * m[7]  * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9]  * m[2] * m[7]  + m[9]  * m[3] * m[6];
	inv[7]  =  m[0] * m[6]  * m[11] - m[0] * m[7]  * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8]  * m[2] * m[7]  - m[8]  * m[3] * m[6];
	inv[11] = -m[0] * m[5]  * m[11] + m[0] * m[7]  * m[9]  + m[4] * m[1] * m[11] - m[4] * m[3] * m[9]  - m[8]  * m[1] * m[7]  + m[8]  * m[3] * m[5];
	inv[15] =  m[0] * m[5]  * m[10] - m[0] * m[6]  * m[9]  - m[4] * m[1] * m[10] + m[4] * m[2] * m[9]  + m[8]  * m[1] * m[6]  - m[8]  * m[2] * m[5];

	const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if (det != 0)
	{
		const float inv_det = 1.0 / det;	
		tmp = mat4_mul_f32(tmp, inv_det);
		optional_set(out_matrix, tmp);
	}

	return out_matrix;
}

void mat4_test_inverse()
{
	Mat4 matrix = {
		.d = {
			12.0f, 2.0f, 1.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 1.0f,
			0.0f, 1.0f, 5.0f, 1.0f,
			11.0f, 1.0f, 0.0f, 10.0f
		},
	};

	optional(Mat4) inverse = mat4_inverse(matrix);
	assert(optional_is_set(inverse));

	Mat4 result = mat4_mul_mat4(matrix, optional_get(inverse));	
	for (i32 row = 0; row < 4; ++row)
	{
		for (i32 col = 0; col < 4; ++col)
		{
			assert(float_nearly_equal(result.d[row][col], mat4_identity.d[row][col]));
		}
	}

	printf("ALL TESTS PASSED!\n");
}
