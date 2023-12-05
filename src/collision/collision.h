#pragma once

// Primitive Collision routines. For convex collision, see mpr.h

#include "math/basic_math.h"
#include "types.h"
#include "math/vec.h"
#include "math/matrix.h"

typedef struct Plane
{
    Vec3 n; // normal
    f32 d; // dot(n,p) for plane
} Plane;

typedef struct LineSegment
{
    Vec3 start;
    Vec3 end;
} LineSegment;

typedef struct Sphere
{
    Vec3 center;
    f32 radius;
} Sphere;

typedef struct Capsule
{
	Vec3 center;
	Quat orientation;
	f32 half_height;
    f32 radius;
} Capsule;

typedef struct OBB
{
    Vec3 center;
	Quat orientation;
    f32 halfwidths[3];
} OBB;

typedef enum ColliderType
{
	COLLIDER_TYPE_SPHERE	= 1 << 0,
	COLLIDER_TYPE_CAPSULE	= 1 << 1,
	COLLIDER_TYPE_OBB 		= 1 << 2,
} ColliderType;

typedef enum ColliderPair 
{
	COLLIDER_PAIR_SPHERE_SPHERE 	= COLLIDER_TYPE_SPHERE	| COLLIDER_TYPE_SPHERE,
	COLLIDER_PAIR_CAPSULE_CAPSULE	= COLLIDER_TYPE_CAPSULE	| COLLIDER_TYPE_CAPSULE,
	COLLIDER_PAIR_OBB_OBB			= COLLIDER_TYPE_OBB		| COLLIDER_TYPE_OBB,
	COLLIDER_PAIR_SPHERE_CAPSULE 	= COLLIDER_TYPE_SPHERE	| COLLIDER_TYPE_CAPSULE,
	COLLIDER_PAIR_SPHERE_OBB 		= COLLIDER_TYPE_SPHERE	| COLLIDER_TYPE_OBB,
	COLLIDER_PAIR_CAPSULE_OBB		= COLLIDER_TYPE_CAPSULE | COLLIDER_TYPE_OBB,
} ColliderPair;

typedef struct Collider
{
	ColliderType type;
	union {
		Sphere sphere;
		Capsule capsule;
		OBB obb;
	};
} Collider;

typedef struct HitResult
{
	Vec3 hit_location_a;
	Vec3 hit_location_b;
	f32 hit_depth;
} HitResult;

LineSegment capsule_segment(const Capsule in_capsule)
{
	Vec3 direction_vector = quat_rotate_vec3(in_capsule.orientation, vec3_new(0, in_capsule.half_height, 0));
	return (LineSegment) {
		.start = vec3_add(in_capsule.center, direction_vector),
		.end = vec3_sub(in_capsule.center, direction_vector),
	};
}

void obb_make_axes(const OBB in_obb, Vec3 out_axes[3])
{
	static const Vec3 x_axis = {
		.x = 1, .y = 0, .z = 0,
	};
	static const Vec3 y_axis = {
		.x = 0, .y = 1, .z = 0,
	};
	static const Vec3 z_axis = {
		.x = 0, .y = 0, .z = 1,
	};

	out_axes[0] = quat_rotate_vec3(in_obb.orientation, x_axis);
	out_axes[1] = quat_rotate_vec3(in_obb.orientation, y_axis);
	out_axes[2] = quat_rotate_vec3(in_obb.orientation, z_axis);
}

f32 distance_point_to_plane(const Vec3 point, const Plane plane)
{
    // return Dot(q, p.n) - p.d; if plane equation normalized (||p.n|| == 1)
    return (vec3_dot(plane.n, point) - plane.d) / vec3_dot(plane.n, plane.n);
}

f32 closest_points_between_line_segments(const LineSegment a, const LineSegment b, Vec3* out_point_a, Vec3* out_point_b)
{
	assert(out_point_a && out_point_b);

	Vec3 slope_a = vec3_sub(a.end, a.start);
	Vec3 slope_b = vec3_sub(b.end, b.start);
	Vec3 r = vec3_sub(a.start, b.start);
	f32 length_a = vec3_length_squared(slope_a);
	f32 length_b = vec3_length_squared(slope_b);
	f32 f = vec3_dot(slope_b, r);

	if (f32_nearly_zero(length_a) && f32_nearly_zero(length_b))
	{
		*out_point_a = a.start;
		*out_point_b = b.start;
	}
	else
	{
		f32 s,t;
		if (f32_nearly_zero(length_a))
		{
			s = 0.0f;
			t = CLAMP(f / length_b, 0.0, 1.0);
		}
		else
		{
			f32 c = vec3_dot(slope_a, r);	
			if (f32_nearly_zero(length_b))
			{
				s = CLAMP(-c / length_a, 0.0, 1.0); 
				t = 0.0f;
			}
			else
			{
				f32 b = vec3_dot(slope_a, slope_b);
				f32 denom = (length_a * length_b) - (b * b);
				s = (denom == 0.0f) ? 0.0f : CLAMP((b*f - c*length_b) / denom, 0.0f, 1.0f);
				t = (b*s + f) / length_b;
				if (t < 0.0f) 
				{
					t = 0.0f;
					s = CLAMP(-c / length_a, 0.0f, 1.0f);
				}
				else if (t > 1.0f) 
				{
					t = 1.0f;
					s = CLAMP((b - c) / length_a, 0.0f, 1.0f);
				}
			}
		}

		*out_point_a = vec3_add(a.start, vec3_scale(slope_a, s));
		*out_point_b = vec3_add(b.start, vec3_scale(slope_b, t));
	}

	return vec3_length(vec3_sub(*out_point_a, *out_point_b));
}

Vec3 closest_point_between_point_and_plane(const Vec3 point, const Plane plane)
{
    f32 distance = distance_point_to_plane(point, plane);
    return vec3_sub(point, vec3_scale(plane.n, distance));
}

Vec3 closest_point_between_point_and_segment(const Vec3 point, const LineSegment segment)
{
    Vec3 ab = vec3_sub(segment.end, segment.start);
    // Project c onto ab, computing parameterized position d(t) = a + t*(b – a)
    f32 t = vec3_dot(vec3_sub(point, segment.start), ab) / vec3_dot(ab, ab);
    // If outside segment, clamp t (and therefore d) to the closest endpoint
	t = CLAMP(t, 0.0f, 1.0f);
    // Compute projected position from the clamped t
    const Vec3 result = vec3_add(segment.start, vec3_scale(ab, t));
	return result;
}

Vec3 closest_point_on_obb_to_point(const OBB obb, const Vec3 point)
{
    Vec3 out_point = obb.center;

	Vec3 obb_axes[3] = {0};
	obb_make_axes(obb, obb_axes);

    Vec3 dist_to_center = vec3_sub(point, obb.center);
    for (int i = 0; i < 3; ++i)
    {
        f32 dist = vec3_dot(dist_to_center, obb_axes[i]);
        const f32 current_halfwidth = obb.halfwidths[i];
        if (dist > current_halfwidth)
        {
            dist = current_halfwidth;
        }
        else if (dist < -current_halfwidth)
        {
            dist = -current_halfwidth;
        }
        out_point = vec3_add(out_point, vec3_scale(obb_axes[i], dist));
    }
    return out_point;
}

f32 squared_distance_point_obb(Vec3 point, OBB obb, Vec3* point_on_obb)
{
    const Vec3 closest = closest_point_on_obb_to_point(obb, point);
	if (point_on_obb)
	{
		*point_on_obb = closest;
	}
    const Vec3 closest_sub_point = vec3_sub(closest, point);
    const f32 squared_distance = vec3_dot(closest_sub_point, closest_sub_point);
    return squared_distance;
}

f32 distance_point_obb(Vec3 point, OBB obb, Vec3* point_on_obb)
{
    return sqrt(squared_distance_point_obb(point, obb, point_on_obb));
}

bool hit_test_sphere_sphere(const Sphere a, const Sphere b, HitResult* out_hit_result)
{
	const Vec3 b_to_a = vec3_sub(a.center, b.center);
	const f32 distance = vec3_length(b_to_a);
	const bool did_hit = distance <= (a.radius + b.radius);

	// If we passed in a hit result struct, set that data now
	if (did_hit && out_hit_result)
	{
		//FCS TODO: This math can be simplified. don't need a_to_b to calc hit_location_a
		const Vec3 a_to_b = vec3_sub(b.center, a.center);
		const Vec3 hit_location_a = vec3_scale(vec3_normalize(a_to_b), a.radius);
		const Vec3 hit_location_b = vec3_scale(vec3_normalize(b_to_a), b.radius);
		const f32 hit_depth = (a.radius + b.radius) - distance;

		(*out_hit_result) = (HitResult) {
			.hit_location_a = hit_location_a,
			.hit_location_b = hit_location_b,
			.hit_depth = hit_depth,
		};
	}

	return did_hit;
}

bool hit_test_capsule_capsule(const Capsule a, const Capsule b, HitResult* out_hit_result)
{
	// Find closest points between the two segments
	Vec3 point_a, point_b;
	f32 distance = closest_points_between_line_segments(capsule_segment(a), capsule_segment(b), &point_a, &point_b);

	// Those points are our sphere centers
	Sphere sphere_a = {
			.center = point_a,
			.radius = a.radius,
		};

	Sphere sphere_b = {
			.center = point_b,
			.radius = b.radius,
		};

	// Now we just need to test 2 spheres
	return hit_test_sphere_sphere(sphere_a, sphere_b, out_hit_result);
}

bool hit_test_obb_obb(const OBB a, const OBB b, HitResult* out_hit_result)
{
	Vec3 a_axes[3] = {0};
	obb_make_axes(a, a_axes);

	Vec3 b_axes[3] = {0};
	obb_make_axes(b, b_axes);

	Mat4 r = mat4_identity;
	for (i32 i = 0; i < 3; ++i)
	{
		for (i32 j = 0; j < 3; ++j)
		{
			r.d[i][j] = vec3_dot(a_axes[i], b_axes[j]); //FCS TODO: Verify this isn't transposed...
		}
	}

	Vec3 a_to_b = vec3_sub(b.center, a.center);
	Mat4 abs_r = mat4_identity;
	static const f32 epsilon = 0.0001f;
	for (i32 i = 0; i < 3; ++i)
	{
		for (i32 j = 0; j < 3; ++j)
		{
			abs_r.d[i][j] = ABS(r.d[i][j]) + epsilon;	
		}
	} 

	f32 t[3] = 
	{ 
		vec3_dot(a_to_b, a_axes[0]), 
		vec3_dot(a_to_b, a_axes[1]), 
		vec3_dot(a_to_b, a_axes[2])
	};

	f32 ra,rb;

	// Test axes L = A0, L = A1, L = A2
	for (i32 i = 0; i < 3; i++)
	{
		ra = a.halfwidths[i];
		rb = b.halfwidths[0] * abs_r.d[i][0] + b.halfwidths[1] * abs_r.d[i][1] + b.halfwidths[2] * abs_r.d[i][2];
		if (ABS(t[i]) > ra + rb) { return false; }
	}
	// Test axes L = B0, L = B1, L = B2
	for (i32 i = 0; i < 3; i++)
	{
		ra = a.halfwidths[0] * abs_r.d[0][i] + a.halfwidths[1] * abs_r.d[1][i] + a.halfwidths[2] * abs_r.d[2][i];
		rb = b.halfwidths[i];
		if (ABS(t[0] * r.d[0][i] + t[1] * r.d[1][i] + t[2] * r.d[2][i]) > ra + rb) { return false; }
	}

	// Test axis L = A0 x B0
	ra = a.halfwidths[1] * abs_r.d[2][0] + a.halfwidths[2] * abs_r.d[1][0];
	rb = b.halfwidths[1] * abs_r.d[0][2] + b.halfwidths[2] * abs_r.d[0][1];
	if (ABS(t[2] * r.d[1][0] - t[1] * r.d[2][0]) > ra + rb) { return false; }
	// Test axis L = A0 x B1
	ra = a.halfwidths[1] * abs_r.d[2][1] + a.halfwidths[2] * abs_r.d[1][1];
	rb = b.halfwidths[0] * abs_r.d[0][2] + b.halfwidths[2] * abs_r.d[0][0];
	if (ABS(t[2] * r.d[1][1] - t[1] * r.d[2][1]) > ra + rb) { return false; }
	// Test axis L = A0 x B2
	ra = a.halfwidths[1] * abs_r.d[2][2] + a.halfwidths[2] * abs_r.d[1][2];
	rb = b.halfwidths[0] * abs_r.d[0][1] + b.halfwidths[1] * abs_r.d[0][0];
	if (ABS(t[2] * r.d[1][2] - t[1] * r.d[2][2]) > ra + rb) { return false; }
	// Test axis L = A1 x B0
	ra = a.halfwidths[0] * abs_r.d[2][0] + a.halfwidths[2] * abs_r.d[0][0];
	rb = b.halfwidths[1] * abs_r.d[1][2] + b.halfwidths[2] * abs_r.d[1][1];
	if (ABS(t[0] * r.d[2][0] - t[2] * r.d[0][0]) > ra + rb) { return false; }
	// Test axis L = A1 x B1
	ra = a.halfwidths[0] * abs_r.d[2][1] + a.halfwidths[2] * abs_r.d[0][1];
	rb = b.halfwidths[0] * abs_r.d[1][2] + b.halfwidths[2] * abs_r.d[1][0];
	if (ABS(t[0] * r.d[2][1] - t[2] * r.d[0][1]) > ra + rb) { return false; }
	// Test axis L = A1 x B2
	ra = a.halfwidths[0] * abs_r.d[2][2] + a.halfwidths[2] * abs_r.d[0][2];
	rb = b.halfwidths[0] * abs_r.d[1][1] + b.halfwidths[1] * abs_r.d[1][0];
	if (ABS(t[0] * r.d[2][2] - t[2] * r.d[0][2]) > ra + rb) { return false; }
	// Test axis L = A2 x B0
	ra = a.halfwidths[0] * abs_r.d[1][0] + a.halfwidths[1] * abs_r.d[0][0];
	rb = b.halfwidths[1] * abs_r.d[2][2] + b.halfwidths[2] * abs_r.d[2][1];
	if (ABS(t[1] * r.d[0][0] - t[0] * r.d[1][0]) > ra + rb) { return false; }
	// Test axis L = A2 x B1
	ra = a.halfwidths[0] * abs_r.d[1][1] + a.halfwidths[1] * abs_r.d[0][1];
	rb = b.halfwidths[0] * abs_r.d[2][2] + b.halfwidths[2] * abs_r.d[2][0];
	if (ABS(t[1] * r.d[0][1] - t[0] * r.d[1][1]) > ra + rb) { return false; }
	// Test axis L = A2 x B2
	ra = a.halfwidths[0] * abs_r.d[1][2] + a.halfwidths[1] * abs_r.d[0][2];
	rb = b.halfwidths[0] * abs_r.d[2][1] + b.halfwidths[1] * abs_r.d[2][0];
	if (ABS(t[1] * r.d[0][2] - t[0] * r.d[1][2]) > ra + rb) { return false; }

	if (out_hit_result)
	{
		Vec3 hit_location_a = closest_point_on_obb_to_point(a, b.center);
		Vec3 hit_location_b = closest_point_on_obb_to_point(b, a.center);
		float hit_depth = vec3_length(vec3_sub(hit_location_a, hit_location_b));

		(*out_hit_result) = (HitResult) {
			.hit_location_a = hit_location_a,
			.hit_location_b = hit_location_b,
			.hit_depth = hit_depth,
		};
	}

	return true;	
}

bool hit_test_sphere_obb(const Sphere sphere, const OBB obb, HitResult* out_hit_result)
{
	Vec3 closest_point_on_obb = {};
    const f32 distance = distance_point_obb(sphere.center, obb, &closest_point_on_obb);

	if (out_hit_result)
	{
		const Vec3 sphere_to_closest_point = vec3_normalize(vec3_sub(closest_point_on_obb, sphere.center));
		const Vec3 sphere_to_obb_center = vec3_normalize(vec3_sub(obb.center, sphere.center));

		//FCS TODO: Can we just check if inside or outside OBB... (hit test point obb)
		const bool flip_sign = vec3_dot(sphere_to_closest_point, sphere_to_obb_center) < 0.0f;	
		const Vec3 direction = flip_sign ? vec3_negate(sphere_to_closest_point) : sphere_to_closest_point;	

		const Vec3 hit_location_a = vec3_scale(direction, sphere.radius);
		const Vec3 hit_location_b = closest_point_on_obb;
		(*out_hit_result) = (HitResult) {
			.hit_location_a = hit_location_a,
			.hit_location_b = hit_location_b,
			.hit_depth = vec3_length(vec3_sub(hit_location_a, hit_location_b)), 
		};
	}

    return distance <= sphere.radius;
}

bool hit_test_sphere_capsule(const Sphere sphere, const Capsule capsule, HitResult* out_hit_result)
{
    Vec3 closest_point_on_segment_to_sphere_center = closest_point_between_point_and_segment(sphere.center, capsule_segment(capsule));
	const Sphere capsule_sphere = {
		.center = closest_point_on_segment_to_sphere_center,
		.radius = capsule.radius,
	};
	return hit_test_sphere_sphere(capsule_sphere, sphere, out_hit_result);
}

bool hit_test_capsule_obb(const Capsule capsule, const OBB obb, HitResult* out_hit_result)
{
    Vec3 closest_point_on_segment_to_obb_center = closest_point_between_point_and_segment(obb.center, capsule_segment(capsule));
    const Sphere capsule_sphere = {
        .center = closest_point_on_segment_to_obb_center,
        .radius = capsule.radius,
    };
    return hit_test_sphere_obb(capsule_sphere, obb, out_hit_result);
}

//FCS TODO: Pass all specific colliders in by ptr above...

bool hit_test_colliders(const Collider* a, const Collider* b, HitResult* out_hit_result)
{
	assert(a && b);		

	ColliderType type_a = a->type;
	ColliderType type_b = b->type;
	ColliderPair collider_pair = type_a | type_b;
	switch (collider_pair)
	{
		case COLLIDER_PAIR_SPHERE_SPHERE: 
		{
			return hit_test_sphere_sphere(a->sphere, b->sphere, out_hit_result);
		}
		case COLLIDER_PAIR_CAPSULE_CAPSULE:
		{
			return hit_test_capsule_capsule(a->capsule, b->capsule, out_hit_result);
		}
		case COLLIDER_PAIR_OBB_OBB:
		{
			return hit_test_obb_obb(a->obb, b->obb, out_hit_result);
		}
		case COLLIDER_PAIR_SPHERE_CAPSULE:
		{
			return type_a < type_b	?	hit_test_sphere_capsule(a->sphere, b->capsule, out_hit_result)
									:	hit_test_sphere_capsule(b->sphere, a->capsule, out_hit_result);
		}
		case COLLIDER_PAIR_SPHERE_OBB:
		{
			return type_a < type_b	?	hit_test_sphere_obb(a->sphere, b->obb, out_hit_result)
									:	hit_test_sphere_obb(b->sphere, a->obb, out_hit_result);
		}
		case COLLIDER_PAIR_CAPSULE_OBB:
		{
			return type_a < type_b	?	hit_test_capsule_obb(a->capsule, b->obb, out_hit_result)
									:	hit_test_capsule_obb(b->capsule, a->obb, out_hit_result);
		}
		default: assert(false); return false;
	}
}

Mat4 collider_compute_transform(const Collider* in_collider)
{
	switch(in_collider->type)
	{
		case COLLIDER_TYPE_SPHERE:
		{
			return mat4_translation(in_collider->sphere.center);
		}
		case COLLIDER_TYPE_CAPSULE:
		{
			return mat4_mul_mat4(quat_to_mat4(in_collider->capsule.orientation), mat4_translation(in_collider->capsule.center));
		}
		case COLLIDER_TYPE_OBB:
		{
			return mat4_mul_mat4(quat_to_mat4(in_collider->obb.orientation), mat4_translation(in_collider->obb.center));
		}
		default: assert(false);
	}
}

void collider_set_orientation(Collider* in_collider, const Quat in_orientation)
{
	switch(in_collider->type)
	{
		case COLLIDER_TYPE_SPHERE:
		{
			break;
		}
		case COLLIDER_TYPE_CAPSULE:
		{
			in_collider->capsule.orientation = in_orientation;
			break;
		}
		case COLLIDER_TYPE_OBB:
		{
			in_collider->obb.orientation = in_orientation;
			break;
		}
		default: assert(false);
	}
}

Quat collider_get_orientation(const Collider* in_collider)
{
	switch(in_collider->type)
	{
		case COLLIDER_TYPE_SPHERE:
		{
			return quat_identity;
		}
		case COLLIDER_TYPE_CAPSULE:
		{
			return in_collider->capsule.orientation;
		}
		case COLLIDER_TYPE_OBB:
		{
			return in_collider->obb.orientation;
		}
		default: assert(false);
	}
}

// FCS TODO: Capsule character moving through collider world
// FCS TODO: primitive collision penetration depth
// FCS TODO: MPR collision penetration depth

