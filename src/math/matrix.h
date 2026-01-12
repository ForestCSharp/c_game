#pragma once

#include "assert.h"
#include "basic_math.h"
#include "basic_types.h"
#define _USE_MATH_DEFINES
#include "vec.h"
#include "quat.h"
#include <math.h>

typedef struct Mat3
{
    union
    {
        Vec3 columns[3];
        float d[3][3];
    };
} Mat3;

declare_optional_type(Mat3);

const Mat3 mat3_identity = {
    .d = {	
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    },
};

const Mat3 mat3_zero = {
    .d = {	
        0, 0, 0,
        0, 0, 0,
        0, 0, 0
    },
};

static inline void mat3_print(const Mat3 m)
{
    printf("[");
    for (i32 col = 0; col < 3; ++col)
    {
        if (col > 0) printf(" ");
        for (i32 row = 0; row < 3; ++row)
        {
            printf("%f", m.d[col][row]);
            if (col < 2 || row < 2) printf(", ");
        }
        if (col < 2) printf("\n");
    }
    printf("]\n");
}

Mat3 mat3_mul_mat3(const Mat3 a, const Mat3 b)
{
    Mat3 result = {0};
    for (i32 col = 0; col < 3; ++col)
    {
    	for (i32 row = 0; row < 3; ++row)
        {
            for (i32 i = 0; i < 3; ++i)
            {
                result.d[col][row] += a.d[col][i] * b.d[i][row];
            }
        }
    }
    return result;
}

Mat3 mat3_add_mat3(const Mat3 a, const Mat3 b)
{
    Mat3 result = {0};
	for (i32 col = 0; col < 3; ++col)
	{
		result.columns[col] = vec3_add(a.columns[col], b.columns[col]);
	}
    return result;
}

Vec3 mat3_mul_vec3(const Mat3 m, const Vec3 v)
{
    return (Vec3){
        .x = m.d[0][0] * v.x + m.d[1][0] * v.y + m.d[2][0] * v.z,
        .y = m.d[0][1] * v.x + m.d[1][1] * v.y + m.d[2][1] * v.z,
        .z = m.d[0][2] * v.x + m.d[1][2] * v.y + m.d[2][2] * v.z,
    };
}

Mat3 mat3_mul_f32(const Mat3 m, const float f)
{
    Mat3 result = m;

    for (i32 col = 0; col < 3; ++col)
	{
    	for (i32 row = 0; row < 3; ++row)
		{
            result.d[col][row] *= f;
		}
	}
    return result;
}

Mat3 mat3_transpose(const Mat3 in_mat)
{
    Mat3 result = {};
    for (i32 col = 0; col < 3; ++col)
	{
        for (i32 row = 0; row < 3; ++row)
		{
            result.d[col][row] = in_mat.d[row][col];
		}
	}
    return result;
}

float mat3_determinant(const Mat3 m_in)
{
    const float* m = (const float*)m_in.d;

    return m[0] * (m[4] * m[8] - m[5] * m[7]) -
           m[1] * (m[3] * m[8] - m[5] * m[6]) +
           m[2] * (m[3] * m[7] - m[4] * m[6]);
}

Mat3 mat3_adjoint(const Mat3 m_in)
{
    Mat3 res = {0};
    const float* m = (const float*)m_in.d;
    float* adj = (float*)res.d;

    // The indices here are pre-transposed to form the Adjoint
    // Row 0
    adj[0] =  (m[4] * m[8] - m[5] * m[7]);
    adj[3] = -(m[3] * m[8] - m[5] * m[6]);
    adj[6] =  (m[3] * m[7] - m[4] * m[6]);

    // Row 1
    adj[1] = -(m[1] * m[8] - m[2] * m[7]);
    adj[4] =  (m[0] * m[8] - m[2] * m[6]);
    adj[7] = -(m[0] * m[7] - m[1] * m[6]);

    // Row 2
    adj[2] =  (m[1] * m[5] - m[2] * m[4]);
    adj[5] = -(m[0] * m[5] - m[2] * m[3]);
    adj[8] =  (m[0] * m[4] - m[1] * m[3]);

    return res;
}

optional(Mat3) mat3_inverse(Mat3 in_mat)
{
    optional(Mat3) out_matrix = {};

    const float determinant = mat3_determinant(in_mat);
    if (determinant != 0.0f)
    {
        const float inverse_determinant = 1.0f / determinant;
        Mat3 result = mat3_mul_f32(mat3_adjoint(in_mat), inverse_determinant);
        optional_set(out_matrix, result);
    }

    return out_matrix;
}

float mat3_minor(const Mat3 m, i32 row, i32 col)
{
    // Create a 2x2 submatrix by skipping the specified row and column
    Mat3 sub = {0};
    i32 sub_row = 0;
    for (i32 i = 0; i < 3; ++i)
    {
        if (i == row) { continue; }
        i32 sub_col = 0;
        for (i32 j = 0; j < 3; ++j)
        {
            if (j == col) { continue; }
            sub.d[sub_row][sub_col] = m.d[i][j];
            sub_col++;
        }
        sub_row++;
    }
    // The minor is the determinant of the 2x2 submatrix
    return (sub.d[0][0] * sub.d[1][1]) - (sub.d[0][1] * sub.d[1][0]);
}

float mat3_cofactor(const Mat3 m, i32 row, i32 col)
{
    float minor = mat3_minor(m, row, col);
    return ((row + col) % 2 == 0) ? minor : -minor;
}

bool mat3_nearly_equal(const Mat3 a, const Mat3 b)
{
    for (i32 column_idx = 0; column_idx < 3; ++column_idx)
    {
        if (!vec3_nearly_equal(a.columns[column_idx], b.columns[column_idx]))
		{
            return false;
		}
    }
    return true;
}

Mat3 mat3_lerp(float t, const Mat3 a, const Mat3 b)
{
    t = CLAMP(t, 0.0f, 1.0f);
    Mat3 result = {};
    for (i32 col = 0; col < 3; ++col)
	{
        result.columns[col] = vec3_lerp(t, a.columns[col], b.columns[col]);
	}
    return result;
}

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

const Mat4 mat4_zero = {
    .d = {	
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0, 
		0, 0, 0, 0
	},
};

static inline void mat4_print(const Mat4 m)
{
    printf("[");
    for (i32 col = 0; col < 4; ++col)
    {
        if (col > 0)
        {
            printf(" ");
        }

    	for (i32 row = 0; row < 4; ++row)
        {
            printf("%f", m.d[col][row]);
            if (row < 3 || col < 3)
            {
                printf(", ");
            }
        }
        if (col < 3)
        {
            printf("\n");
        }
    }
    printf("]\n");
}

Mat4 mat4_mul_mat4(const Mat4 a, const Mat4 b)
{
    Mat4 result = {0};

    for (i32 col = 0; col < 4; ++col)
    {
    	for (i32 row = 0; row < 4; ++row)
        {
            for (i32 i = 0; i < 4; ++i)
            {
                result.d[col][row] += a.d[col][i] * b.d[i][row];
            }
        }
    }

    return result;
}

Vec4 mat4_mul_vec4(const Mat4 m, const Vec4 v)
{
    return (Vec4){
        .x = m.d[0][0] * v.x + m.d[1][0] * v.y + m.d[2][0] * v.z + m.d[3][0] * v.w,
        .y = m.d[0][1] * v.x + m.d[1][1] * v.y + m.d[2][1] * v.z + m.d[3][1] * v.w,
        .z = m.d[0][2] * v.x + m.d[1][2] * v.y + m.d[2][2] * v.z + m.d[3][2] * v.w,
        .w = m.d[0][3] * v.x + m.d[1][3] * v.y + m.d[2][3] * v.z + m.d[3][3] * v.w,
    };
}

Mat4 mat4_mul_f32(const Mat4 m, const float f)
{
	Mat4 result = m;
    for (i32 col = 0; col < 4; ++col)
    {
		for (i32 row = 0; row < 4; ++row)
        {
	 		result.d[col][row] *= f; 
		}
	}
	return result;
}

Mat4 mat4_transpose(const Mat4 in_mat)
{
	Mat4 result = {};
	for (i32 col = 0; col < 4; ++col)
	{
		for (i32 row = 0; row < 4; ++row)
		{
			result.d[col][row] = in_mat.d[row][col];
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

Mat4 mat4_from_axes(const Vec3 in_forward, const Vec3 in_up)
{
	const Vec3 right = vec3_normalize(vec3_cross(in_forward, in_up));
	const Vec3 forward = vec3_normalize(vec3_cross(in_up, right)); 
	const Vec3 up = vec3_normalize(vec3_cross(right, forward)); 

	return (Mat4) {
		.d = {
			right.x,	right.y,	right.z,	0,
			up.x, 		up.y, 		up.z, 		0,
			-forward.x,	-forward.y,	-forward.z,	0,
			0,			0,			0,			1,
		},
	};
}

Mat4 mat4_look_at(const Vec3 from, const Vec3 to, const Vec3 in_up)
{
	const Vec3 f = vec3_normalize(vec3_sub(to, from));
	const Vec3 s = vec3_normalize(vec3_cross(f, in_up));
	const Vec3 u = vec3_cross(s,f);

	return (Mat4) {
		.d = {
			s.x, 				u.x, 				-f.x, 				0,
			s.y, 				u.y, 				-f.y, 				0,
			s.z, 				u.z, 				-f.z, 				0,
			-vec3_dot(s, from), -vec3_dot(u, from), vec3_dot(f, from), 	1
		},
	};
}

Mat4 mat4_perspective(const float fov, const float aspect_ratio, const float frustum_near, const float frustum_far)
{
    float D2R = M_PI / 180.0f;
    float y_scale = 1.0 / tan(D2R * fov / 2);
    float x_scale = y_scale / aspect_ratio;
    float near_minus_far = frustum_near - frustum_far;

    return (Mat4) {
        .d = {
			x_scale, 	0, 				0, 													0, 
			0, 			y_scale,		0, 													0, 
			0, 			0, 				(frustum_far + frustum_near) / near_minus_far,		-1, 
			0, 			0, 				2 * frustum_far * frustum_near / near_minus_far,	0
		},
    };
}

Vec3 mat4_get_translation(const Mat4 in_mat)
{
	return vec3_new(in_mat.d[3][0], in_mat.d[3][1], in_mat.d[3][2]);
}

float mat4_determinant(const Mat4 m_in)
{
    const float* m = (const float*)m_in.d;

    // We only need the first 4 cofactors to calculate the determinant
    float c0 =  m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    float c4 = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    float c8 =  m[4] * m[9]  * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    float c12 = -m[4] * m[9]  * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    return m[0] * c0 + m[1] * c4 + m[2] * c8 + m[3] * c12;
}

Mat4 mat4_adjoint(const Mat4 m_in)
{
    Mat4 res = {0};
    const float* m = (const float*)m_in.d;
    float* adj = (float*)res.d;

    adj[0]  =  m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    adj[4]  = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    adj[8]  =  m[4] * m[9]  * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    adj[12] = -m[4] * m[9]  * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    
    adj[1]  = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    adj[5]  =  m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    adj[9]  = -m[0] * m[9]  * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    adj[13] =  m[0] * m[9]  * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    
    adj[2]  =  m[1] * m[6]  * m[15] - m[1] * m[7]  * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7]  - m[13] * m[3] * m[6];
    adj[6]  = -m[0] * m[6]  * m[15] + m[0] * m[7]  * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7]  + m[12] * m[3] * m[6];
    adj[10] =  m[0] * m[5]  * m[15] - m[0] * m[7]  * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7]  - m[12] * m[3] * m[5];
    adj[14] = -m[0] * m[5]  * m[14] + m[0] * m[6]  * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6]  + m[12] * m[2] * m[5];
    
    adj[3]  = -m[1] * m[6]  * m[11] + m[1] * m[7]  * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9]  * m[2] * m[7]  + m[9]  * m[3] * m[6];
    adj[7]  =  m[0] * m[6]  * m[11] - m[0] * m[7]  * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8]  * m[2] * m[7]  - m[8]  * m[3] * m[6];
    adj[11] = -m[0] * m[5]  * m[11] + m[0] * m[7]  * m[9]  + m[4] * m[1] * m[11] - m[4] * m[3] * m[9]  - m[8]  * m[1] * m[7]  + m[8]  * m[3] * m[5];
    adj[15] =  m[0] * m[5]  * m[10] - m[0] * m[6]  * m[9]  - m[4] * m[1] * m[10] + m[4] * m[2] * m[9]  + m[8]  * m[1] * m[6]  - m[8]  * m[2] * m[5];

    return res;
}

optional(Mat4) mat4_inverse(Mat4 in_mat)
{
    optional(Mat4) out_matrix = {};

    const float determinant = mat4_determinant(in_mat);
    if (determinant != 0.0f)
    {
        const float inverse_determinant = 1.0f / determinant;
        Mat4 result = mat4_mul_f32(mat4_adjoint(in_mat), inverse_determinant);
        optional_set(out_matrix, result);
    }

    return out_matrix;
}

float mat4_minor(const Mat4 m, i32 row, i32 col)
{
    // Create a 3x3 submatrix by skipping the specified row and column
    Mat3 sub = {0};
    i32 sub_row = 0;
    for (i32 i = 0; i < 4; ++i)
    {
        if (i == row) { continue; }
        i32 sub_col = 0;
        for (i32 j = 0; j < 4; ++j)
        {
            if (j == col) { continue; }
            sub.d[sub_row][sub_col] = m.d[i][j];
            sub_col++;
        }
        sub_row++;
    }
    // The minor is the determinant of the 3x3 submatrix
    return mat3_determinant(sub);
}

float mat4_cofactor(const Mat4 m, i32 row, i32 col)
{
    float minor = mat4_minor(m, row, col);
    return ((row + col) % 2 == 0) ? minor : -minor;
}

bool mat4_nearly_equal(const Mat4 a, const Mat4 b)
{
	for (i32 column_idx = 0; column_idx < 4; ++column_idx)
	{
		if (!vec4_nearly_equal(a.columns[column_idx], b.columns[column_idx]))
		{
			return false;
		}
	}

	return true;
}

Mat4 mat4_lerp(float t, const Mat4 a, const Mat4 b)
{
	t = CLAMP(t, 0.0f, 1.0f);
	Mat4 result = {};
	for (i32 col = 0; col < 4; ++col)
	{
		result.columns[col] = vec4_lerp(t, a.columns[col], b.columns[col]);
	}
	return result;
}

Mat4 mat3_to_mat4(const Mat3 m)
{
    return (Mat4) {
        .d = {
            m.d[0][0], m.d[0][1], m.d[0][2], 0.0f,
            m.d[1][0], m.d[1][1], m.d[1][2], 0.0f,
            m.d[2][0], m.d[2][1], m.d[2][2], 0.0f,
            0.0f,      0.0f,      0.0f,      1.0f
        }
    };
}

