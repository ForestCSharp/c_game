#pragma once

#include "matrix.h"
#include "vec.h"
#include <math.h>

typedef struct Quat
{
    float x;
    float y;
    float z;
    float w;
} Quat;

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
    axis    = vec3_normalize(axis);
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

// TODO: quat_inverse: If the quaternion q = <w, v> is a unit quaternion, then its inverse is just its conjugate: q{-1}
// = <w, -v> (just negate x,y,z)

// (s1,v1) * (s2, v2) = (s1s2 - v1 dot v2, s1v2 + s2v1 + v1 cross v2)

Quat quat_mult(const Quat q1, const Quat q2)
{
    return (Quat){
        .x = q1.x * q2.w + q1.y * q2.z - q1.z * q2.y + q1.w * q2.x,
        .y = -q1.x * q2.z + q1.y * q2.w + q1.z * q2.x + q1.w * q2.y,
        .z = q1.x * q2.y - q1.y * q2.x + q1.z * q2.w + q1.w * q2.z,
        .w = -q1.x * q2.x - q1.y * q2.y - q1.z * q2.z + q1.w * q2.w,
    };
}

Quat quat_slerp(const Quat a, const Quat b, const float t)
{
    float theta     = acosf(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
    float sin_theta = sinf(theta);
    float wa        = sinf((1 - t) * theta) / sin_theta;
    float wb        = sinf(t * theta) / sin_theta;
    Quat r          = {
                 .x = wa * a.x + wb * b.x,
                 .y = wa * a.y + wb * b.y,
                 .z = wa * a.z + wb * b.z,
                 .w = wa * a.w + wb * b.w,
    };
    r = quat_normalize(r);
    return r;
}

Mat4 quat_to_mat4(const Quat in_q)
{
    Quat q = in_q;

    // TODO: zero_element quat check

    // check normalized
    if (fabsf(quat_size_squared(q) - 1.0f) > 0.001)
    {
        printf("quat_to_mat4 error: quaternion should be normalized \n");
        q = quat_normalize(q);
    }

    float x  = q.x;
    float x2 = x * x;
    float y  = q.y;
    float y2 = y * y;
    float z  = q.z;
    float z2 = z * z;
    float w  = q.w;
    float w2 = w * w;

    float xy = x * y;
    float wx = w * x;
    float xz = x * z;
    float wy = w * y;
    float yz = y * z;
    float wz = w * z;

    return (Mat4){
        1 - 2 * y2 - 2 * z2,
        2 * xy - 2 * wz,
        2 * xz + 2 * wy,
        0,
        2 * xy + 2 * wz,
        1 - 2 * x2 - 2 * z2,
        2 * yz - 2 * wx,
        0,
        2 * xz - 2 * wy,
        2 * yz + 2 * wx,
        1 - 2 * x2 - 2 * y2,
        0,
        0,
        0,
        0,
        1,
    };
}
