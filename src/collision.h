#pragma once

// Primitive Collision routines. For convex collision, see mpr.h

#include "types.h"
#include "vec.h"

typedef struct Plane
{
    Vec3 n; // normal
    float d; // dot(n,p) for plane
} Plane;

typedef struct LineSegment
{
    Vec3 start;
    Vec3 end;
} LineSegment;

typedef struct OBB
{
    Vec3 center;
    Vec3 axes[3];
    float halfwidths[3];
} OBB;

typedef struct Sphere
{
    Vec3 center;
    float radius;
} Sphere;

typedef struct Capsule
{
    LineSegment segment;
    float radius;
} Capsule;

float distance_point_to_plane(const Vec3 point, const Plane plane)
{
    // return Dot(q, p.n) - p.d; if plane equation normalized (||p.n||==1)
    return (vec3_dot(plane.n, point) - plane.d) / vec3_dot(plane.n, plane.n);
}

Vec3 closest_point_between_point_and_plane(const Vec3 point, const Plane plane)
{
    float distance = distance_point_to_plane(point, plane);
    return vec3_sub(point, vec3_scale(plane.n, distance));
}

Vec3 closest_point_between_point_and_segment(const Vec3 point, const LineSegment segment)
{
    Vec3 ab = vec3_sub(segment.end, segment.start);
    // Project c onto ab, computing parameterized position d(t) = a + t*(b – a)
    float t = vec3_dot(vec3_sub(point, segment.start), ab) / vec3_dot(ab, ab);
    // If outside segment, clamp t (and therefore d) to the closest endpoint
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t > 1.0f)
    {
        t = 1.0f;
    }
    // Compute projected position from the clamped t
    return vec3_add(segment.start, vec3_scale(ab, t));
}

// FCS TODO: Rename: returns closest point ON or IN the OBB
Vec3 closest_point_on_obb_to_point(const OBB obb, const Vec3 point)
{
    Vec3 out_point = obb.center;

    Vec3 dist_to_center = vec3_sub(point, obb.center);
    for (int i = 0; i < 3; ++i)
    {
        float dist = vec3_dot(dist_to_center, obb.axes[i]);
        const float current_halfwidth = obb.halfwidths[i];
        if (dist > current_halfwidth)
        {
            dist = current_halfwidth;
        }
        else if (dist < -current_halfwidth)
        {
            dist = -current_halfwidth;
        }
        out_point = vec3_add(out_point, vec3_scale(obb.axes[i], dist));
    }
    return out_point;
}

float squared_distance_point_obb(Vec3 point, OBB obb)
{
    const Vec3 closest = closest_point_on_obb_to_point(obb, point);
    const Vec3 closest_sub_point = vec3_sub(closest, point);
    const float squared_distance = vec3_dot(closest_sub_point, closest_sub_point);
    return squared_distance;
}

float distance_point_obb(Vec3 point, OBB obb)
{
    return sqrt(squared_distance_point_obb(point, obb));
}

// bool hit_test_point_obb(const Vec3 point, const OBB obb)
// {
// TODO: distance_point_obb -> nearly zero
// }

bool hit_test_sphere_obb(const Sphere sphere, const OBB obb)
{
    const float distance = distance_point_obb(sphere.center, obb);
    return distance <= sphere.radius;
}

bool hit_test_capsule_obb(const Capsule capsule, const OBB obb)
{
    Vec3 closest_point_on_segment_to_obb_center = closest_point_between_point_and_segment(obb.center, capsule.segment);
    const Sphere sphere = {
        .center = closest_point_on_segment_to_obb_center,
        .radius = capsule.radius,
    };
    return hit_test_sphere_obb(sphere, obb);
}

// FCS TODO: computing hit depth
//  i.e. for sphere ->
//       if sphere center outside OBB, figure out how much of the radius is inside the OBB
//       if sphere center inside OBB, compute distance to OBB boundary PLUS radius

void test_collision()
{
    Sphere sphere_a = {
        .center = vec3_new(5, 0, 0),
        .radius = 1.0f,
    };

    OBB obb_a = {
        .center = vec3_new(3, 3, 3),
        .axes =
            {
                vec3_new(1, 0, 0),
                vec3_new(0, 1, 0),
                vec3_new(0, 0, 1),
            },
        .halfwidths = {5, 5, 5},
    };

    const bool hit_result_sphere_a_obb_a = hit_test_sphere_obb(sphere_a, obb_a);
    printf("hit_result_sphere_a_obb_a: %s \n", hit_result_sphere_a_obb_a ? "hit" : "no hit");

    Capsule capsule_a = {
        .segment =
            {
                .start = vec3_new(-10, -10, -10),
                .end = vec3_new(10, 10, 10),
            },
        .radius = 2.0f,
    };

    const bool hit_result_capsule_a_obb_a = hit_test_capsule_obb(capsule_a, obb_a);
    printf("hit_result_capsule_a_obb_a: %s \n", hit_result_capsule_a_obb_a ? "hit" : "no hit");

    // FCS TODO: Test an OBB that's actually oriented

    // exit(0);
}

// FCS TODO: Capsule character moving through OBB-based world
// FCS TODO: primitive collision penetration depth
// FCS TODO: MPR collision penetration depth
