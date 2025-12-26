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
	}
}
