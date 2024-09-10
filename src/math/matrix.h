#pragma once

#include "assert.h"
#include "basic_math.h"
#include "types.h"
#define _USE_MATH_DEFINES
#include "vec.h"
#include "quat.h"
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
        .x = m.d[0][0] * v.x + m.d[1][0] * v.y + m.d[2][0] * v.z + m.d[3][0] * v.w,
        .y = m.d[0][1] * v.x + m.d[1][1] * v.y + m.d[2][1] * v.z + m.d[3][1] * v.w,
        .z = m.d[0][2] * v.x + m.d[1][2] * v.y + m.d[2][2] * v.z + m.d[3][2] * v.w,
        .w = m.d[0][3] * v.x + m.d[1][3] * v.y + m.d[2][3] * v.z + m.d[3][3] * v.w,
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

Mat4 mat4_transpose(const Mat4 in_mat)
{
	Mat4 result = {};
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			result.d[i][j] = in_mat.d[j][i];
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
			x_scale, 	0, 				0, 									0, 
			0, 			y_scale * -1.0, 0, 									0, 
			0, 			0, 				(frustum_far + frustum_near) / near_minus_far,		-1, 
			0, 			0, 				2 * frustum_far * frustum_near / near_minus_far,	0
		},
    };
}

Vec3 mat4_get_translation(const Mat4 in_mat)
{
	return vec3_new(in_mat.d[3][0], in_mat.d[3][1], in_mat.d[3][2]);
}

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
	for (i32 i = 0; i < 4; ++i)
	{
		result.columns[i] = vec4_lerp(t, a.columns[i], b.columns[i]);
	}
	return result;
}

Mat4 quat_to_mat4(const Quat in_q)
{
    Quat q = in_q;

    // check normalized
    if (fabsf(quat_size_squared(q) - 1.0f) > 0.001)
    {
        printf("quat_to_mat4 error: quaternion should be normalized \n");
        q = quat_normalize(q);
    }

    float x = q.x;		float x2 = x * x;
    float y = q.y;		float y2 = y * y;
    float z = q.z;		float z2 = z * z;
    float w = q.w;		float w2 = w * w;

    float xy = x * y;	float wy = w * y;
    float wx = w * x;   float yz = y * z;
    float xz = x * z;   float wz = w * z; 

	return (Mat4) {
		.d = {
			1 - 2 * y2 - 2 * z2,	2 * xy + 2 * wz, 		2 * xz - 2 * wy, 		0,
			2 * xy - 2 * wz,		1 - 2 * x2 - 2 * z2,	2 * yz + 2 * wx, 		0,
			2 * xz + 2 * wy, 		2 * yz - 2 * wx, 		1 - 2 * x2 - 2 * y2,	0,
			0, 						0, 						0, 						1
		}
	};
}

Quat mat4_to_quat(const Mat4 in_mat)
{
	const float tr = in_mat.d[0][0] + in_mat.d[1][1] + in_mat.d[2][2];
	if (tr > 0)
	{ 
		const float S = sqrt(tr+1.0) * 2; // S=4*qw 
		return (Quat) {
			.x = (in_mat.d[1][2] - in_mat.d[2][1]) / S,
			.y = (in_mat.d[2][0] - in_mat.d[0][2]) / S,
			.z = (in_mat.d[0][1] - in_mat.d[1][0]) / S,
			.w = 0.25 * S,
		};
	}
	else if ((in_mat.d[0][0] > in_mat.d[1][1]) && (in_mat.d[0][0] > in_mat.d[2][2]))
	{ 
		float S = sqrt(1.0 + in_mat.d[0][0] - in_mat.d[1][1] - in_mat.d[2][2]) * 2; // S=4*qx 
		return (Quat) {
			.x = 0.25 * S,
			.y = (in_mat.d[1][0] + in_mat.d[0][1]) / S,
			.z = (in_mat.d[2][0] + in_mat.d[0][2]) / S,
			.w = (in_mat.d[1][2] - in_mat.d[2][1]) / S,
		};
	}
	else if (in_mat.d[1][1] > in_mat.d[2][2])
	{ 
		float S = sqrt(1.0 + in_mat.d[1][1] - in_mat.d[0][0] - in_mat.d[2][2]) * 2; // S=4*qy
		return (Quat) {
			.x = (in_mat.d[1][0] + in_mat.d[0][1]) / S,
			.y = 0.25 * S,
			.z = (in_mat.d[2][1] + in_mat.d[1][2]) / S,
			.w = (in_mat.d[2][0] - in_mat.d[0][2]) / S,
		};
	}
	else
	{ 
		float S = sqrt(1.0 + in_mat.d[2][2] - in_mat.d[0][0] - in_mat.d[1][1]) * 2; // S=4*qz
		return (Quat) {
			.x = (in_mat.d[2][0] + in_mat.d[0][2]) / S,
			.y = (in_mat.d[2][1] + in_mat.d[1][2]) / S,
			.z = 0.25 * S,
			.w = (in_mat.d[0][1] - in_mat.d[1][0]) / S,
		};
		// qx = (m02 + m20) / S;
		// qy = (m12 + m21) / S;
		// qz = 0.25 * S;
		// qw = (m10 - m01) / S;
	}
}

typedef struct TRS
{
	Vec3 scale;
	Quat rotation;
	Vec3 translation;
} TRS;

const TRS trs_identity = {
	.scale = {.x = 1, .y = 1, .z = 1},
	.rotation = {.x = 0, .y = 0, .z = 0, .w = 1},
	.translation = {.x = 0, .y = 0, .z = 0},
};

TRS mat4_to_trs(Mat4 in_mat)
{
	TRS out_trs;

	out_trs.translation = vec4_xyz(in_mat.columns[3]);
	
	out_trs.scale.x = vec3_length(vec4_xyz(in_mat.columns[0]));
	out_trs.scale.y = vec3_length(vec4_xyz(in_mat.columns[1]));
	out_trs.scale.z = vec3_length(vec4_xyz(in_mat.columns[2]));
	
	const Mat4 rotation_matrix = {
		.columns = {
			vec4_scale(in_mat.columns[0], 1.0f / out_trs.scale.x),
			vec4_scale(in_mat.columns[1], 1.0f / out_trs.scale.y),	
			vec4_scale(in_mat.columns[2], 1.0f / out_trs.scale.z),
			vec4_new(0,0,0,1),
		}
	};
	out_trs.rotation = mat4_to_quat(rotation_matrix);

	return out_trs;
}

Mat4 trs_to_mat4(const TRS trs)
{
	return mat4_mul_mat4(
		mat4_mul_mat4(mat4_scale(trs.scale), quat_to_mat4(trs.rotation)), 
		mat4_translation(trs.translation)
	);
}

TRS trs_lerp(float t, const TRS a, const TRS b)
{
	return (TRS) {
		.scale = vec3_lerp(t, a.scale, b.scale),
		.rotation = quat_nlerp(t, a.rotation, b.rotation),
		.translation = vec3_lerp(t, a.translation, b.translation),
	};
}

TRS trs_combine(const TRS parent, const TRS child)
{
	TRS out_trs = {};
	out_trs.scale = vec3_mul_componentwise(parent.scale, child.scale);

	const Quat scaled_parent_rotation = quat_normalize(quat_mul(parent.rotation, mat4_to_quat(mat4_scale(parent.scale))));
	out_trs.rotation = quat_normalize(quat_mul(scaled_parent_rotation, child.rotation));

	const Vec3 scaled_translation = vec3_mul_componentwise(parent.scale, child.translation);
	const Vec3 rotated_translation = quat_rotate_vec3(parent.rotation, scaled_translation);
	out_trs.translation = vec3_add(parent.translation, rotated_translation);

	return out_trs;
}

