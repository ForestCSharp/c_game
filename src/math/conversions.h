#pragma once 

#include "vec.h"
#include "quat.h"
#include "matrix.h"

//FCS TODO: Move other conversions here

Mat3 quat_to_mat3(const Quat in_q)
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

	return (Mat3) {
		.d = {
			1 - 2 * y2 - 2 * z2,	2 * xy + 2 * wz, 		2 * xz - 2 * wy,
			2 * xy - 2 * wz,		1 - 2 * x2 - 2 * z2,	2 * yz + 2 * wx,
			2 * xz + 2 * wy, 		2 * yz - 2 * wx, 		1 - 2 * x2 - 2 * y2
		}
	};
}

Mat4 quat_to_mat4(const Quat in_q)
{
	return mat3_to_mat4(quat_to_mat3(in_q));
}

// Hamilton products on vectors in (x, y, z, w) order: L(a)b = R(b)a = a*b.
Mat4 quat_left_matrix(Quat q)
{
	return (Mat4){ .d = {
		{ q.w,  q.z, -q.y, -q.x},
		{-q.z,  q.w,  q.x, -q.y},
		{ q.y, -q.x,  q.w, -q.z},
		{ q.x,  q.y,  q.z,  q.w},
	} };
}

Mat4 quat_right_matrix(Quat q)
{
	return (Mat4){ .d = {
		{ q.w, -q.z,  q.y, -q.x},
		{ q.z,  q.w, -q.x, -q.y},
		{-q.y,  q.x,  q.w, -q.z},
		{ q.x,  q.y,  q.z,  q.w},
	} };
}

Quat mat4_to_quat(const Mat4 in_mat)
{
	const float tr = in_mat.d[0][0] + in_mat.d[1][1] + in_mat.d[2][2];
	if (tr > 0)
	{ 
		const float s = sqrt(tr+1.0) * 2; // S=4*qw 
		return (Quat) {
			.x = (in_mat.d[1][2] - in_mat.d[2][1]) / s,
			.y = (in_mat.d[2][0] - in_mat.d[0][2]) / s,
			.z = (in_mat.d[0][1] - in_mat.d[1][0]) / s,
			.w = 0.25 * s,
		};
	}
	else if ((in_mat.d[0][0] > in_mat.d[1][1]) && (in_mat.d[0][0] > in_mat.d[2][2]))
	{ 
		float s = sqrt(1.0 + in_mat.d[0][0] - in_mat.d[1][1] - in_mat.d[2][2]) * 2; // S=4*qx 
		return (Quat) {
			.x = 0.25 * s,
			.y = (in_mat.d[1][0] + in_mat.d[0][1]) / s,
			.z = (in_mat.d[2][0] + in_mat.d[0][2]) / s,
			.w = (in_mat.d[1][2] - in_mat.d[2][1]) / s,
		};
	}
	else if (in_mat.d[1][1] > in_mat.d[2][2])
	{ 
		float s = sqrt(1.0 + in_mat.d[1][1] - in_mat.d[0][0] - in_mat.d[2][2]) * 2; // S=4*qy
		return (Quat) {
			.x = (in_mat.d[1][0] + in_mat.d[0][1]) / s,
			.y = 0.25 * s,
			.z = (in_mat.d[2][1] + in_mat.d[1][2]) / s,
			.w = (in_mat.d[2][0] - in_mat.d[0][2]) / s,
		};
	}
	else
	{ 
		float s = sqrt(1.0 + in_mat.d[2][2] - in_mat.d[0][0] - in_mat.d[1][1]) * 2; // S=4*qz
		return (Quat) {
			.x = (in_mat.d[2][0] + in_mat.d[0][2]) / s,
			.y = (in_mat.d[2][1] + in_mat.d[1][2]) / s,
			.z = 0.25 * s,
			.w = (in_mat.d[0][1] - in_mat.d[1][0]) / s,
		};
	}
}
