#pragma once

#include "vec.h"
#include <math.h>

typedef struct Quat
{
    float x;
    float y;
    float z;
    float w;
} Quat;

declare_optional_type(Quat);

const Quat quat_identity = {
    .x = 0,
    .y = 0,
    .z = 0,
    .w = 1,
};

void quat_print(const Quat q)
{
    printf("[%f %f %f] [%f] ", q.x, q.y, q.z, q.w);
}

// Construct quaternion from axis angle representation
Quat quat_new(Vec3 axis, float angle)
{
    axis = vec3_normalize(axis);
    float s = sinf(angle / 2.0f);
    return (Quat){
        .x = axis.x * s,
        .y = axis.y * s,
        .z = axis.z * s,
        .w = cosf(angle / 2.0f),
    };
}

Quat quat_scale(const Quat q, float s)
{
    return (Quat){
        .x = q.x * s,
        .y = q.y * s,
        .z = q.z * s,
        .w = q.w * s,
    };
}

float quat_size_squared(const Quat q)
{
    return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
}

float quat_size(const Quat q)
{
    return sqrtf(quat_size_squared(q));
}

Quat quat_normalize(const Quat q)
{
    // FIXME: check size != 0
    float size = quat_size(q);
    return quat_scale(q, 1.0f / size);
}

Quat quat_conjugate(const Quat q)
{
	return (Quat) {
		.x = -q.x,
		.y = -q.y,
		.z = -q.z,
		.w = q.w,
	};
}

Quat quat_mul(const Quat q1, const Quat q2)
{
    return (Quat){
        .x = q1.x * q2.w + q1.y * q2.z - q1.z * q2.y + q1.w * q2.x,
        .y = -q1.x * q2.z + q1.y * q2.w + q1.z * q2.x + q1.w * q2.y,
        .z = q1.x * q2.y - q1.y * q2.x + q1.z * q2.w + q1.w * q2.z,
        .w = -q1.x * q2.x - q1.y * q2.y - q1.z * q2.z + q1.w * q2.w,
    };
}

float quat_dot(const Quat a, const Quat b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Quat quat_add(const Quat a, const Quat b)
{
	return (Quat) {
		.x = a.x + b.x,
		.y = a.y + b.y,
		.z = a.z + b.z,
		.w = a.w + b.w,
	};
}

Quat quat_nlerp(float t, const Quat a, const Quat b)
{
	t = CLAMP(t, 0.0f, 1.0f);
	return quat_add(quat_scale(a,1.0f - t), quat_scale(b, t));	
}

Quat quat_slerp(float t, Quat a, Quat b)
{
	t = CLAMP(t, 0.0f, 1.0f);
	float dot = quat_dot(a,b);

	// Take the shortest path
	if (dot < 0.0f)
	{
		a = quat_scale(a, -1.0f);
		dot *= -1.0f;
	}
	
	// Ensure dot is within acosf's valid range
	dot = CLAMP(dot, -1.0f, 1.0f);

    const float angle = acosf(dot);
    const float denom = sinf(angle);
	
	if (f32_nearly_equal(denom, 0.0f))
	{
		return b;
	}

	Quat a_scaled = quat_scale(a, sin((1-t) * angle));
	Quat b_scaled = quat_scale(b, sin(t * angle));
	Quat result = quat_scale(quat_add(a_scaled, b_scaled), 1.0f / denom);

	return quat_normalize(result);
}

bool quat_nearly_equal(const Quat a, const Quat b)
{
	return 	f32_nearly_equal(a.x, b.x)
		&&	f32_nearly_equal(a.y, b.y)
		&&	f32_nearly_equal(a.z, b.z)
		&&	f32_nearly_equal(a.w, b.w);
}

Vec3 quat_rotate_vec3(const Quat q, const Vec3 v)
{
	const Quat vector_quat = {
		.x = v.x,
		.y = v.y,
		.z = v.z,
		.w = 0.0f,
	};

	const Quat q_conjugate = quat_conjugate(q);

	const Quat result_quat = quat_mul(quat_mul(q, vector_quat), q_conjugate);

	return vec3_new(result_quat.x, result_quat.y, result_quat.z);
}

Vec3 quat_to_forward_vec3(const Quat q)
{
	Vec3 v = vec3_new(0,0,1);
	return quat_rotate_vec3(q, v);
}

Quat quat_look_rotation(const Vec3 in_forward, const Vec3 in_up)
{
 		const Vec3 forward = vec3_normalize(in_forward);
        const Vec3 right = vec3_normalize(vec3_cross(in_up, forward));
        const Vec3 up = vec3_cross(forward, right);

        const float m00 = right.x;
        const float m01 = right.y;
        const float m02 = right.z;
        const float m10 = up.x;
        const float m11 = up.y;
        const float m12 = up.z;
        const float m20 = forward.x;
        const float m21 = forward.y;
        const float m22 = forward.z;


        float num8 = (m00 + m11) + m22;
		Quat quaternion = {};
        if (num8 > 0.0f)
        {
            float num = sqrtf(num8 + 1.0f);
            quaternion.w = num * 0.5f;
            num = 0.5f / num;
            quaternion.x = (m12 - m21) * num;
            quaternion.y = (m20 - m02) * num;
            quaternion.z = (m01 - m10) * num;
            return quaternion;
        }
        if ((m00 >= m11) && (m00 >= m22))
        {
            float num7 = sqrtf(((1.0f + m00) - m11) - m22);
            float num4 = 0.5f / num7;
            quaternion.x = 0.5f * num7;
            quaternion.y = (m01 + m10) * num4;
            quaternion.z = (m02 + m20) * num4;
            quaternion.w = (m12 - m21) * num4;
            return quaternion;
        }
        if (m11 > m22)
        {
            float num6 = sqrtf(((1.0f + m11) - m00) - m22);
            float num3 = 0.5f / num6;
            quaternion.x = (m10 + m01) * num3;
            quaternion.y = 0.5f * num6;
            quaternion.z = (m21 + m12) * num3;
            quaternion.w = (m20 - m02) * num3;
            return quaternion;
        }

        float num5 = sqrtf(((1.0f + m22) - m00) - m11);
        float num2 = 0.5f / num5;
        quaternion.x = (m20 + m02) * num2;
        quaternion.y = (m21 + m12) * num2;
        quaternion.z = 0.5f * num5;
        quaternion.w = (m01 - m10) * num2;
        return quaternion;
}
