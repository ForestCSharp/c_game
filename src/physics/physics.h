#pragma once

#include "basic_types.h"
#include "math/math_lib.h"
#include "stretchy_buffer.h"
#include "physics/convex_helpers.h"

typedef enum ShapeType
{
	SHAPE_TYPE_SPHERE,
	SHAPE_TYPE_BOX,
	SHAPE_TYPE_CONVEX,
} ShapeType;

typedef struct SphereShape
{
	f32 radius;
} SphereShape;

const i32 NUM_BOX_POINTS = 8;

typedef struct BoxShape
{
	Vec3 points[NUM_BOX_POINTS];
	Bounds bounds;
	Vec3 center_of_mass;
} BoxShape;

// creates a box from some arbitrary number of points by expanding a bounding box
BoxShape box_shape_create(const Vec3 in_extents)
{
	const f32 w = in_extents.x;
	const f32 h = in_extents.y;
	const f32 d = in_extents.z;

	Vec3 box_points[NUM_BOX_POINTS] = {
		vec3_new(	-w,	-h, d),
		vec3_new(	 w,	-h, d),
		vec3_new(	-w,	 h,	d),
		vec3_new(	 w,	 h,	d),

		vec3_new(	-w,	-h, -d),
		vec3_new(	 w,	-h, -d),
		vec3_new(	-w,	 h,	-d),
		vec3_new(	 w,	 h,	-d),
	};

	Bounds bounds = bounds_init();
	bounds_expand_points(&bounds, box_points, ARRAY_COUNT(box_points));

	const Vec3 center_of_mass = vec3_scale(vec3_add(bounds.min, bounds.max), 0.5f);
	
	return (BoxShape) {
		.points = {
			vec3_new(bounds.min.x, bounds.min.y, bounds.min.z),
			vec3_new(bounds.max.x, bounds.min.y, bounds.min.z),
			vec3_new(bounds.min.x, bounds.max.y, bounds.min.z),
			vec3_new(bounds.min.x, bounds.min.y, bounds.max.z),

			vec3_new(bounds.max.x, bounds.max.y, bounds.max.z),
			vec3_new(bounds.min.x, bounds.max.y, bounds.max.z),
			vec3_new(bounds.max.x, bounds.min.y, bounds.max.z),
			vec3_new(bounds.max.x, bounds.max.y, bounds.min.z)
		},
		.bounds = bounds,
		.center_of_mass = center_of_mass,
	};
}

typedef struct ConvexShape
{
	ConvexHull hull;
	Bounds bounds;
	Mat3 inertia_tensor;
	Vec3 center_of_mass;
} ConvexShape;

#define USE_MONTE_CARLO_CALCULATION 1

ConvexShape convex_shape_create(const Vec3* in_points, const i32 in_num_points)
{
	ConvexHull hull = convex_hull_create(in_points, in_num_points);

	Bounds bounds = bounds_init();
	bounds_expand_points(&bounds, in_points, in_num_points);

	#if USE_MONTE_CARLO_CALCULATION
	const Mat3 inertia_tensor = convex_hull_calculate_inertia_tensor_monte_carlo(&hull);
	const Vec3 center_of_mass = convex_hull_calculate_center_of_mass_monte_carlo(&hull);
	#else 
	const Mat3 inertia_tensor = convex_hull_calculate_inertia_tensor(&hull);
	const Vec3 center_of_mass = convex_hull_calculate_center_of_mass(&hull);
	#endif // USE_MONTE_CARLO_CALCULATION

	return (ConvexShape) {
		.hull = hull,
		.bounds = bounds,
		.inertia_tensor = inertia_tensor,
		.center_of_mass = center_of_mass,
	};
}

typedef struct Shape
{
	ShapeType type;	
	union
	{
		SphereShape sphere;
		BoxShape box;
		ConvexShape convex;
	};
} Shape;

Mat3 shape_get_inertia_tensor_matrix(Shape* in_shape)
{
	switch (in_shape->type)
	{
		case SHAPE_TYPE_SPHERE:			
		{
			const SphereShape sphere = in_shape->sphere;
			const f32 sphere_radius = sphere.radius;
			const f32 sphere_radius_squared = sphere_radius * sphere_radius;
			const f32 sphere_tensor_value = (2.0f / 5.0f) * sphere_radius_squared;
			Mat3 sphere_tensor = {
				.d[0][0] = sphere_tensor_value,
				.d[1][1] = sphere_tensor_value,
				.d[2][2] = sphere_tensor_value,
			};
			return sphere_tensor;
		}
		case SHAPE_TYPE_BOX:
		{	
			const BoxShape box = in_shape->box;
			const f32 dx = box.bounds.max.x - box.bounds.min.x;
			const f32 dy = box.bounds.max.y - box.bounds.min.y;
			const f32 dz = box.bounds.max.z - box.bounds.min.z;

			const f32 dx2 = dx * dx;
			const f32 dy2 = dy * dy;
			const f32 dz2 = dz * dz;

			// Inertia tensor for box centered at (0,0,0)
			Mat3 box_tensor = {
				.d[0][0] = (dy2 + dz2) / 12.0f,
				.d[1][1] = (dx2 + dz2) / 12.0f,
				.d[2][2] = (dx2 + dy2) / 12.0f,
			};

			// Use parallel axis theorem to handle box not centered at origin
			const Vec3 cm = vec3_new(
				(box.bounds.max.x + box.bounds.min.x) * 0.5f,
				(box.bounds.max.y + box.bounds.min.y) * 0.5f,
				(box.bounds.max.z + box.bounds.min.z) * 0.5f
			);

			const Vec3 r = vec3_sub(vec3_zero, cm);
			const f32 r_l2 = vec3_length_squared(r);

			Mat3 pat_tensor = {
				.columns[0] =	vec3_new(r_l2 - r.x * r.x,	r.x * r.y,			r.x * r.z),
				.columns[1] =	vec3_new(r.y * r.x, 		r_l2 - r.y * r.y,	r.y * r.z),
				.columns[2] =	vec3_new(r.z * r.x, 		r.z * r.y,			r_l2 - r.z * r.z)
			};

			return mat3_add_mat3(box_tensor, pat_tensor);
		}
		case SHAPE_TYPE_CONVEX:
		{
			return in_shape->convex.inertia_tensor;
		}
	}
	assert(false);
}

typedef struct PhysicsBody
{
	Vec3 position;
	Quat orientation;
	Vec3 linear_velocity;
	Vec3 angular_velocity;
	Shape shape;
	f32 inverse_mass;
	f32 elasticity;
	f32 friction;

	Vec3 debug_color;

} PhysicsBody;

typedef struct PhysicsContact
{
	Vec3 point_on_a_world;	
	Vec3 point_on_b_world;
	Vec3 point_on_a_local;
	Vec3 point_on_b_local;

	Vec3 normal;
	f32 separation_distance;
	f32 time_of_impact;

	PhysicsBody* body_a;
	PhysicsBody* body_b;
} PhysicsContact;


// Returns point on a convex shape that's furthest in a particular direction
Vec3 physics_body_support(const PhysicsBody* in_body, const Vec3 in_dir, const f32 in_bias)
{	
	switch(in_body->shape.type)
	{
		case SHAPE_TYPE_SPHERE:
		{
			const SphereShape* sphere = &in_body->shape.sphere;
			return vec3_add(in_body->position, vec3_scale(in_dir, sphere->radius + in_bias));
		}
		case SHAPE_TYPE_BOX:
		{
			const BoxShape* box = &in_body->shape.box;
			Vec3 max_point = vec3_add(quat_rotate_vec3(in_body->orientation, box->points[0]), in_body->position);
			f32 max_distance = vec3_dot(in_dir, max_point);

			for (i32 point_idx = 1; point_idx < NUM_BOX_POINTS; ++point_idx)
			{
				const Vec3 current_point = vec3_add(quat_rotate_vec3(in_body->orientation, box->points[point_idx]), in_body->position);
				const f32 current_distance = vec3_dot(in_dir, current_point);
				if (current_distance > max_distance)
				{
					max_point = current_point;
					max_distance = current_distance;
				}
			}

			Vec3 norm = vec3_scale(vec3_normalize(in_dir), in_bias);
			return vec3_add(max_point, norm);	
		}
		case SHAPE_TYPE_CONVEX:
		{
			const ConvexShape* convex = &in_body->shape.convex;
			const ConvexHull* hull = &convex->hull;	
			i32 num_convex_points = sb_count(hull->points);
			assert(num_convex_points > 0);

			Vec3 max_point = vec3_add(quat_rotate_vec3(in_body->orientation, hull->points[0]), in_body->position);
			f32 max_distance = vec3_dot(in_dir, max_point);

			for (i32 point_idx = 1; point_idx < num_convex_points; ++point_idx)
			{
				const Vec3 current_point = vec3_add(quat_rotate_vec3(in_body->orientation, hull->points[point_idx]), in_body->position);
				const f32 current_distance = vec3_dot(in_dir, current_point);
				if (current_distance > max_distance)
				{
					max_point = current_point;
					max_distance = current_distance;
				}
			}

			Vec3 norm = vec3_scale(vec3_normalize(in_dir), in_bias);
			return vec3_add(max_point, norm);	
		}
	}

	assert(false);
	return vec3_zero;
}

/** Calls physics_body_support on both bodies, storing the results as well as their difference */
MinkowskiPoint physics_bodies_support(const PhysicsBody* in_body_a, const PhysicsBody* in_body_b, const Vec3 in_dir, const f32 in_bias)
{
	MinkowskiPoint out_point = {};

	// Find point in body a furthest in dir
	const Vec3 normalized_dir = vec3_normalize(in_dir);	
	out_point.pt_a = physics_body_support(in_body_a, normalized_dir, in_bias);

	// Find point in body b furthest in dir
	const Vec3 reversed_dir = vec3_scale(normalized_dir, -1.0f);
	out_point.pt_b = physics_body_support(in_body_b, reversed_dir, in_bias);

	// xyz is minkowski sum point
	out_point.xyz = vec3_sub(out_point.pt_a, out_point.pt_b);

	return out_point;
}

f32 physics_bodies_epa_expand(
	const PhysicsBody* in_body_a, 
	const PhysicsBody* in_body_b, 
	const f32 in_bias, 
	const MinkowskiPoint in_simplex_points[4],
	Vec3* out_point_on_a,
	Vec3* out_point_on_b
)
{
	sbuffer(MinkowskiPoint) points = NULL;
	sbuffer(ConvexTri) tris = NULL;
	sbuffer(ConvexEdge) dangling_edges = NULL;

	// Add points from in_simplex_points and determine center
	Vec3 center = vec3_zero;
	for (i32 i = 0; i < 4; ++i)
	{
		sb_push(points, in_simplex_points[i]);
		center = vec3_add(center, in_simplex_points[i].xyz);
	}
	center = vec3_scale(center, 0.25f);

	// Build up triangles
	for (i32 i = 0; i < 4; ++i)
	{
		const i32 j = (i + 1) % 4;	
		const i32 k = (i + 2) % 4;

		ConvexTri tri = {
			.a = i,
			.b = j,
			.c = k,
		};
		
		i32 unused_idx = (i + 3) % 4;

		if (signed_distance_to_triangle(tri, points[unused_idx].xyz, points, sb_count(points)) > 0.0f)
		{
			SWAP(i32, tri.a, tri.b);
		}

		sb_push(tris, tri);
	}

	// Expand the simplex to find the closest face of the CSO to the origin
	while (1)
	{
		const i32 idx = closest_triangle(tris, sb_count(tris), points, sb_count(points));
		const Vec3 normal = triangle_normal_direction(tris[idx], points, sb_count(points));
		const MinkowskiPoint new_point = physics_bodies_support(in_body_a, in_body_b, normal, in_bias);

		// if w already exists, we can't expand further
		if (triangle_has_point(new_point.xyz, tris, sb_count(tris), points, sb_count(points)))
		{
			break;	
		}

		// if distance isn't beyond origin, can't expand
		if (signed_distance_to_triangle(tris[idx], new_point.xyz, points, sb_count(points)) <= 0.0f)
		{
			break;
		}

		// Add new point
		const i32 new_idx = sb_count(points);	
		sb_push(points, new_point);

		// Remove triangles that face this point
		i32 num_removed = remove_triangles_facing_point(new_point.xyz, &tris, points, sb_count(points));
		if (num_removed == 0)
		{
			break;
		}

		// Find dangling edges
		find_dangling_edges(tris, sb_count(tris), &dangling_edges);
		if (sb_count(dangling_edges) == 0)
		{
			break;
		}

		for (i32 i = 0; i < sb_count(dangling_edges); ++i)
		{
			const ConvexEdge edge = dangling_edges[i];

			ConvexTri tri = {
				.a = new_idx,
				.b = edge.b,
				.c = edge.a,
			};

			if (signed_distance_to_triangle(tri, center, points, sb_count(points)) > 0.0f)
			{
				SWAP(i32, tri.b, tri.c);
			}

			sb_push(tris, tri);
		}
	}

	const i32 tri_idx = closest_triangle(tris, sb_count(tris), points, sb_count(points));
	const ConvexTri tri = tris[tri_idx];

	const Vec3 lambdas = barycentric_coordinates(
		points[tri.a].xyz, 
		points[tri.b].xyz, 
		points[tri.c].xyz, 
		vec3_zero
	);

	*out_point_on_a = vec3_add(
		vec3_scale(points[tri.a].pt_a, lambdas.v[0]), 
		vec3_add(
			vec3_scale(points[tri.b].pt_a, lambdas.v[1]), 
			vec3_scale(points[tri.c].pt_a, lambdas.v[2])
		)
	);

	*out_point_on_b = vec3_add(
		vec3_scale(points[tri.a].pt_b, lambdas.v[0]), 
		vec3_add(
			vec3_scale(points[tri.b].pt_b, lambdas.v[1]), 
			vec3_scale(points[tri.c].pt_b, lambdas.v[2])
		)
	);

	sb_free(points);
	sb_free(tris);
	sb_free(dangling_edges);

	const Vec3 delta = vec3_sub(*out_point_on_b, *out_point_on_a);
	return vec3_length(delta);
}

const i32 MAX_GJK_ITERATIONS = 64;

bool physics_bodies_gjk_intersect(const PhysicsBody* in_body_a, const PhysicsBody* in_body_b, const f32 in_bias, Vec3* out_pt_on_a, Vec3* out_pt_on_b)
{
	assert(out_pt_on_a != NULL);
	assert(out_pt_on_b != NULL);

	const Vec3 origin = vec3_zero;

	i32 num_points = 1;
	MinkowskiPoint simplex_points[4] = {0};
	simplex_points[0] = physics_bodies_support(in_body_a, in_body_b, vec3_new(1,1,1), 0.0f);

	f32 closest_dist = 1e10f;
	bool contains_origin = false;
	Vec3 new_dir = vec3_scale(simplex_points[0].xyz, -1.0f);

	i32 num_iterations = 0;
	do
	{
		if (++num_iterations > MAX_GJK_ITERATIONS)
		{
			break;
		}

		MinkowskiPoint new_point = physics_bodies_support(in_body_a, in_body_b, new_dir, 0.0f);

		// if new point is same as previous, then we can't expand further
		if (simplex_has_point(simplex_points, num_points, &new_point))
		{
			break;
		}

		// Add new MinkowskiPoint to our array of points
		simplex_points[num_points] = new_point;
		++num_points;

		// if the new point hasn't moved past the origin, then the origin cannot be in the set, so break
		const f32 dot_dot = vec3_dot(new_dir, vec3_sub(new_point.xyz, origin));
		if (dot_dot < 0.0f)
		{
			break;
		}

		// run simplex_signed_volumes, modifying new_dir and lambdas 
		Vec4 lambdas;
		contains_origin = simplex_signed_volumes(simplex_points, num_points, &new_dir, &lambdas);

		// break if we contain the origin
		if (contains_origin)
		{
			break;
		}

		// if new projection isn't closer, break
		f32 dist = vec3_length_squared(new_dir);
		if (dist >= closest_dist)
		{
			break;		
		}

		// Update closest_dist
		closest_dist = dist;

		// Use lambdas that support the new search dir, and invalidate any points that don't support it
		sort_valids(simplex_points, &lambdas);
		num_points = num_valids(lambdas);

		// If we reached 4 total points in our simplex, then we contain the origin and the two bodies intersect
		contains_origin = (num_points == 4);
	}
	while (!contains_origin);

	// Only run EPA on successful intersections
	if (!contains_origin)
	{
		return false;
	}

	// EPA Needs 4 points
	if (num_points == 1)
	{
		const Vec3 search_dir = vec3_negate(simplex_points[0].xyz);
		const MinkowskiPoint new_point = physics_bodies_support(in_body_a, in_body_b, search_dir, 0.0f);
		simplex_points[num_points] = new_point;
		num_points += 1;
	}

	if (num_points == 2)
	{
		const Vec3 ab = vec3_sub(simplex_points[1].xyz, simplex_points[0].xyz);
		Vec3 u,v;
		vec3_get_ortho(ab, &u, &v);

		const Vec3 search_dir = u;
		const MinkowskiPoint new_point = physics_bodies_support(in_body_a, in_body_b, search_dir, 0.0f);
		simplex_points[num_points] = new_point;
		num_points += 1;
	}

	if (num_points == 3)
	{	
		const Vec3 ab = vec3_sub(simplex_points[1].xyz, simplex_points[0].xyz);
		const Vec3 ac = vec3_sub(simplex_points[2].xyz, simplex_points[0].xyz);
		const Vec3 norm = vec3_cross(ab,ac);

		const Vec3 search_dir = norm;
		const MinkowskiPoint new_point = physics_bodies_support(in_body_a, in_body_b, search_dir, 0.0f);
		simplex_points[num_points] = new_point;
		num_points += 1;
	}

	assert(num_points == 4);

	Vec3 avg = vec3_zero;
	for (i32 i = 0; i < num_points; ++i)
	{
		avg = vec3_add(avg, simplex_points[i].xyz);
	}
	avg = vec3_scale(avg, 0.25f);

	// Now expand simplex by bias amount
	for (i32 i = 0; i < num_points; ++i)
	{
		MinkowskiPoint* pt = &simplex_points[i];
		const Vec3 dir = vec3_normalize(vec3_sub(pt->xyz, avg));
		pt->pt_a = vec3_add(pt->pt_a, vec3_scale(dir, in_bias));
		pt->pt_b = vec3_sub(pt->pt_b, vec3_scale(dir, in_bias));
		pt->xyz = vec3_sub(pt->pt_a, pt->pt_b);
	}

	physics_bodies_epa_expand(in_body_a, in_body_b, in_bias, simplex_points, out_pt_on_a, out_pt_on_b);

	return true;
}

void physics_bodies_gjk_closest_points(const PhysicsBody* in_body_a, const PhysicsBody* in_body_b, Vec3* out_point_on_a, Vec3* out_point_on_b)
{
	assert(out_point_on_a != NULL);
	assert(out_point_on_b != NULL);

	const Vec3 origin = vec3_zero;

	f32 closest_dist_squared = 1e10;
	const f32 bias = 0.0f;

	i32 num_points = 1;
	MinkowskiPoint simplex_points[4] = {0};

	simplex_points[0] = physics_bodies_support(in_body_a, in_body_b, vec3_new(1,1,1), bias);

	Vec4 lambdas = vec4_new(1,0,0,0);
	Vec3 new_dir = vec3_negate(simplex_points[0].xyz);

	i32 num_iterations = 0;
	do
	{
		if (++num_iterations > MAX_GJK_ITERATIONS)
		{
			break;
		}

		const MinkowskiPoint new_point = physics_bodies_support(in_body_a, in_body_b, new_dir, bias);

		// if the new point is the same as a previous point: break
		if (simplex_has_point(simplex_points, num_points, &new_point))
		{
			break;
		}

		// Add new point to simplex
		simplex_points[num_points] = new_point;
		num_points += 1;

		// Update new_dir and lambdas
		simplex_signed_volumes(simplex_points, num_points, &new_dir, &lambdas);
		sort_valids(simplex_points, &lambdas);
		num_points = num_valids(lambdas);

		// If we failed to find a closer point: break
		f32 dist_squared = vec3_length_squared(new_dir);
		if (dist_squared >= closest_dist_squared)
		{
			break;	
		}

		// Found a new closest distance
		closest_dist_squared = dist_squared;
	} while (num_points < 4);

	*out_point_on_a = vec3_zero;
	*out_point_on_b = vec3_zero;
	for (i32 i = 0; i < 4; ++i)
	{
		*out_point_on_a = vec3_add(*out_point_on_a, vec3_scale(simplex_points[i].pt_a, lambdas.v[i]));
		*out_point_on_b = vec3_add(*out_point_on_b, vec3_scale(simplex_points[i].pt_b, lambdas.v[i]));
	}
}

f32 physics_body_get_max_linear_speed(const PhysicsBody* in_body, const Vec3 in_angular_velocity, const Vec3 in_dir)
{
	switch(in_body->shape.type)
	{
		case SHAPE_TYPE_SPHERE:
		{
			return 0.f;
		}
		case SHAPE_TYPE_BOX:
		{
			const BoxShape box = in_body->shape.box;
			f32 max_speed = 0.f;
			for (i32 i = 0; i < NUM_BOX_POINTS; ++i)
			{
				const Vec3 r = vec3_sub(box.points[i], box.center_of_mass);
				const Vec3 linear_velocity = vec3_cross(in_angular_velocity, r);
				const f32 point_speed = vec3_dot(in_dir, linear_velocity);
				if (point_speed > max_speed) { max_speed = point_speed; }
			}
			return max_speed;
		}
		case SHAPE_TYPE_CONVEX:
		{
			const ConvexShape convex = in_body->shape.convex;
			f32 max_speed = 0.f;
			for (i32 i = 0; i < sb_count(convex.hull.points); ++i)
			{
				const Vec3 r = vec3_sub(convex.hull.points[i], convex.center_of_mass);
				const Vec3 linear_velocity = vec3_cross(in_angular_velocity, r);
				const f32 point_speed = vec3_dot(in_dir, linear_velocity);
				if (point_speed > max_speed) { max_speed = point_speed; }
			}
			return max_speed;
		}
		default:
		{
			assert(false);
			return 0.f;
		}
	}
}

Bounds physics_body_get_bounds(const PhysicsBody* in_body)
{
	Bounds out_bounds = bounds_init();

	switch(in_body->shape.type)
	{
		case SHAPE_TYPE_SPHERE:
		{
			const SphereShape sphere = in_body->shape.sphere;
			const f32 radius = sphere.radius;
			out_bounds = (Bounds) {
				.min = vec3_sub(in_body->position, vec3_new(radius, radius, radius)),
				.max = vec3_add(in_body->position, vec3_new(radius, radius, radius)),
			};
			break;
		}
		case SHAPE_TYPE_BOX:
		{
			//FCS TODO: bounds_to_world helper
			const BoxShape box = in_body->shape.box;
			Vec3 corners[NUM_BOX_POINTS] =
			{
				vec3_new(box.bounds.min.x, box.bounds.min.y, box.bounds.min.z),
				vec3_new(box.bounds.min.x, box.bounds.min.y, box.bounds.max.z),
				vec3_new(box.bounds.min.x, box.bounds.max.y, box.bounds.min.z),
				vec3_new(box.bounds.max.x, box.bounds.min.y, box.bounds.min.z),

				vec3_new(box.bounds.max.x, box.bounds.max.y, box.bounds.max.z),
				vec3_new(box.bounds.max.x, box.bounds.max.y, box.bounds.min.z),
				vec3_new(box.bounds.max.x, box.bounds.min.y, box.bounds.max.z),
				vec3_new(box.bounds.min.x, box.bounds.max.y, box.bounds.max.z),
			};

			for (i32 i = 0; i < NUM_BOX_POINTS; ++i)
			{
				corners[i] = vec3_add(quat_rotate_vec3(in_body->orientation, corners[i]), in_body->position);
				bounds_expand_point(&out_bounds, corners[i]);
			}
			break;
		}
		case SHAPE_TYPE_CONVEX:
		{
			//FCS TODO: bounds_to_world helper
			const ConvexShape convex = in_body->shape.convex;
			Vec3 corners[NUM_BOX_POINTS] =
			{
				vec3_new(convex.bounds.min.x, convex.bounds.min.y, convex.bounds.min.z),
				vec3_new(convex.bounds.min.x, convex.bounds.min.y, convex.bounds.max.z),
				vec3_new(convex.bounds.min.x, convex.bounds.max.y, convex.bounds.min.z),
				vec3_new(convex.bounds.max.x, convex.bounds.min.y, convex.bounds.min.z),

				vec3_new(convex.bounds.max.x, convex.bounds.max.y, convex.bounds.max.z),
				vec3_new(convex.bounds.max.x, convex.bounds.max.y, convex.bounds.min.z),
				vec3_new(convex.bounds.max.x, convex.bounds.min.y, convex.bounds.max.z),
				vec3_new(convex.bounds.min.x, convex.bounds.max.y, convex.bounds.max.z),
			};

			for (i32 i = 0; i < NUM_BOX_POINTS; ++i)
			{
				corners[i] = vec3_add(quat_rotate_vec3(in_body->orientation, corners[i]), in_body->position);
				bounds_expand_point(&out_bounds, corners[i]);
			}
			break;
		}
		default:
		{
			assert(false);
		}
	}

	return out_bounds;
}

Vec3 physics_body_local_to_world_space(const PhysicsBody* in_body, Vec3 in_body_space_point)
{
	Vec3 rotated = quat_rotate_vec3(in_body->orientation, in_body_space_point);
	Vec3 translated = vec3_add(rotated, in_body->position);
	return translated;
}

Vec3 physics_body_world_to_local_space(const PhysicsBody* in_body, Vec3 in_world_point)
{
	Vec3 untranslated = vec3_sub(in_world_point, in_body->position);
	Vec3 unrotated = quat_rotate_vec3(quat_inverse(in_body->orientation), untranslated);
	return unrotated;
}

Vec3 physics_body_get_center_of_mass_world(const PhysicsBody* in_body)
{
	Vec3 local_space_center_of_mass = vec3_zero;;
	switch (in_body->shape.type)
	{
		case SHAPE_TYPE_SPHERE:			
		{
			local_space_center_of_mass = vec3_zero;
			break;
		}
		case SHAPE_TYPE_BOX:
		{	
			local_space_center_of_mass = in_body->shape.box.center_of_mass;
			break;
		}
		case SHAPE_TYPE_CONVEX:
		{
			local_space_center_of_mass = in_body->shape.convex.center_of_mass;
			break;
		}
	}

	return physics_body_local_to_world_space(in_body, local_space_center_of_mass);
}

Mat3 physics_body_get_inverse_inertia_tensor_local(PhysicsBody* in_body)
{
	Mat3 result = optional_get(mat3_inverse(shape_get_inertia_tensor_matrix(&in_body->shape)));
	result = mat3_mul_f32(result, in_body->inverse_mass);
	return result;
}

Mat3 physics_body_get_inverse_inertia_tensor_world(PhysicsBody* in_body)
{
	Mat3 orientation_matrix = quat_to_mat3(in_body->orientation);
	Mat3 orientation_matrix_transpose = mat3_transpose(orientation_matrix);
	Mat3 inverse_inertia_tensor_local = physics_body_get_inverse_inertia_tensor_local(in_body);
	return mat3_mul_mat3(
		mat3_mul_mat3(
			orientation_matrix, 
			inverse_inertia_tensor_local
		), 
		orientation_matrix_transpose
	);
}

void physics_body_apply_impulse_linear(PhysicsBody* in_body, Vec3 in_impulse)
{
	if (in_body->inverse_mass <= 0.f) { return; }

	// Multiply impulse by inverse_mass
	const Vec3 delta_linear_velocity = vec3_scale(in_impulse, in_body->inverse_mass);
	// Accumulate linear velocity
	in_body->linear_velocity = vec3_add(in_body->linear_velocity, delta_linear_velocity);
}

void physics_body_apply_impulse_angular(PhysicsBody* in_body, Vec3 in_impulse)
{	
	if (in_body->inverse_mass <= 0.f) { return; }

	// Multiply impulse by inertia tensor to get angular velocity
	const Vec3 delta_angular_velocity = mat3_mul_vec3(physics_body_get_inverse_inertia_tensor_world(in_body), in_impulse);
	// Accumulate angular velocity
	in_body->angular_velocity = vec3_add(in_body->angular_velocity, delta_angular_velocity);

	// Clamp Angular Velocity to some max angular speed
	const f32 max_angular_speed = 30.0f;
	const f32 max_angular_speed_squared = max_angular_speed * max_angular_speed;
	if (vec3_length_squared(in_body->angular_velocity) > max_angular_speed_squared)
	{
		in_body->angular_velocity = vec3_scale(vec3_normalize(in_body->angular_velocity), max_angular_speed);
	}
}

void physics_body_apply_impulse(PhysicsBody* in_body, Vec3 in_impulse, Vec3 in_location)
{
	// Apply linear impulse
	physics_body_apply_impulse_linear(in_body, in_impulse);

	// Get center of mass
	const Vec3 center_of_mass = physics_body_get_center_of_mass_world(in_body);	
	// Get direction vector to location from center of mass
	const Vec3 r = vec3_sub(in_location, center_of_mass);
	// Compute angular impulse based on the cross product of that direction vector and your impulse 
	const Vec3 impulse_angular = vec3_cross(r, in_impulse);
	// Apply angular impulse
	physics_body_apply_impulse_angular(in_body, impulse_angular);
}

void physics_body_update(PhysicsBody* in_body, f32 in_delta_time)
{	
	// Update position based on linear velocity
	in_body->position = vec3_add(in_body->position, vec3_scale(in_body->linear_velocity, in_delta_time));

	// Update orientation and position based on angular velocity
	Vec3 center_of_mass = physics_body_get_center_of_mass_world(in_body);
	Vec3 center_of_mass_to_position = vec3_sub(in_body->position, center_of_mass);
	
	// Compute inertia tensor and inverse inertia tensor in world space
	Mat3 orientation_matrix = quat_to_mat3(in_body->orientation);
	Mat3 inertia_tensor = 
		mat3_mul_mat3(
			mat3_mul_mat3(
				orientation_matrix, 
				shape_get_inertia_tensor_matrix(&in_body->shape)
			),
			mat3_transpose(orientation_matrix)	
		);
	optional(Mat3) inverse_inertia_tensor = mat3_inverse(inertia_tensor);
	assert(optional_is_set(inverse_inertia_tensor));

	// Compute torque alpha value
	Vec3 alpha = mat3_mul_vec3(
					optional_get(inverse_inertia_tensor),
					vec3_cross(
						in_body->angular_velocity, 
						mat3_mul_vec3(inertia_tensor, in_body->angular_velocity)
					)
				);

	// Accumulate angular velocity
	in_body->angular_velocity = vec3_add(in_body->angular_velocity, vec3_scale(alpha, in_delta_time));

	// scale angular velocity by timestep
	Vec3 scaled_angular_velocity = vec3_scale(in_body->angular_velocity, in_delta_time);
	// form quaternion using axis and magnitude of scaled_angular_velocity
	Quat angular_velocity_quat = quat_new(scaled_angular_velocity, vec3_length(scaled_angular_velocity));

	// Update orientation and position based on angular velocity
	in_body->orientation = quat_normalize(quat_mul(angular_velocity_quat, in_body->orientation));
	in_body->position = vec3_add(center_of_mass, quat_rotate_vec3(angular_velocity_quat, center_of_mass_to_position));
}

bool ray_sphere_intersect(
	const Vec3 in_ray_start, 
	const Vec3 in_ray_dir, 
	const Vec3 in_sphere_center, 
	const f32 in_sphere_radius, 
	f32* out_t1, 
	f32* out_t2
)
{
	const Vec3 m = vec3_sub(in_sphere_center, in_ray_start);
	const f32 a = vec3_dot(in_ray_dir, in_ray_dir);
	const f32 b = vec3_dot(m, in_ray_dir);
	const f32 c = vec3_dot(m,m) - (in_sphere_radius * in_sphere_radius);
	const f32 delta = b * b - a * c;
	if (delta < 0)
	{
		// no real solutions exist
		return false;
	}
	
	const f32 inv_a = 1.0f / a;
	const f32 delta_root = sqrtf(delta);
	*out_t1 = inv_a * (b - delta_root);	
	*out_t2 = inv_a * (b + delta_root);
	return true;
}

bool physics_body_intersect_sphere_sphere(
	const SphereShape* in_sphere_a,
	const SphereShape* in_sphere_b,
	const Vec3 in_pos_a,
	const Vec3 in_pos_b,
	const Vec3 in_vel_a,
	const Vec3 in_vel_b,
	const f32 in_delta_time,
	Vec3* out_point_on_a,
	Vec3* out_point_on_b,
	f32* out_time_of_impact
)
{
	const Vec3 relative_velocity = vec3_sub(in_vel_a, in_vel_b);
	const Vec3 ray_start = in_pos_a;
	const Vec3 ray_end = vec3_add(ray_start, vec3_scale(relative_velocity, in_delta_time));
	const Vec3 ray_dir = vec3_sub(ray_end, ray_start);

	const Vec3 combined_sphere_pos = in_pos_b;
	const f32 combined_sphere_radius = in_sphere_a->radius + in_sphere_b->radius;

	f32 t0 = 0;
	f32 t1 = 0;

	if (vec3_length_squared(ray_dir) < 0.001f * 0.001f)
	{
		// ray_dir is too short, just check if already intersecting
		const Vec3 a_to_b = vec3_sub(in_pos_b, in_pos_a);
		const f32 radius = combined_sphere_radius + 0.001f;
		const f32 radius_squared = radius * radius;
		if (vec3_length_squared(a_to_b) > radius_squared)
		{
			return false;
		}

	}
	else if (!ray_sphere_intersect(ray_start, ray_dir, combined_sphere_pos, combined_sphere_radius, &t0, &t1))
	{
		return false;
	}

	t0 *= in_delta_time;
	t1 *= in_delta_time;

	if (t1 < 0.0f)
	{
		return false;
	}

	const f32 time_of_impact = (t0 < 0.0f) ? 0.0f : t0;

	if (time_of_impact > in_delta_time)
	{
		return false;
	}

	const Vec3 new_pos_a = vec3_add(in_pos_a, vec3_scale(in_vel_a, time_of_impact));
	const Vec3 new_pos_b = vec3_add(in_pos_b, vec3_scale(in_vel_b, time_of_impact));
	const Vec3 a_to_b_norm = vec3_normalize(vec3_sub(new_pos_b, new_pos_a));

	*out_point_on_a = vec3_add(new_pos_a, vec3_scale(a_to_b_norm, in_sphere_a->radius));
	*out_point_on_b = vec3_sub(new_pos_b, vec3_scale(a_to_b_norm, in_sphere_b->radius));
	*out_time_of_impact = time_of_impact;
	return true;
}

// Checks collision at current point in time for two bodies
bool physics_bodies_intersect(PhysicsBody* in_body_a, PhysicsBody* in_body_b, PhysicsContact* in_contact)
{
	const f32 bias = 0.001f;
	Vec3 pt_on_a;
	Vec3 pt_on_b;
	if (physics_bodies_gjk_intersect(in_body_a, in_body_b, bias, &pt_on_a, &pt_on_b))
	{
		const Vec3 normal = vec3_normalize(vec3_sub(pt_on_b, pt_on_a));

		const Vec3 biased_normal = vec3_scale(normal,bias);
		pt_on_a = vec3_sub(pt_on_a, biased_normal);
		pt_on_b = vec3_add(pt_on_b, biased_normal);

		in_contact->normal = normal;
		in_contact->point_on_a_world = pt_on_a;
		in_contact->point_on_b_world = pt_on_b;
		in_contact->point_on_a_local = physics_body_world_to_local_space(in_contact->body_a, in_contact->point_on_a_world);
		in_contact->point_on_b_local = physics_body_world_to_local_space(in_contact->body_b, in_contact->point_on_b_world);
		in_contact->separation_distance = -vec3_length(vec3_sub(pt_on_a, pt_on_b));
		return true;
	}

	physics_bodies_gjk_closest_points(in_body_a, in_body_b, &pt_on_a, &pt_on_b);	
	in_contact->point_on_a_world = pt_on_a;
	in_contact->point_on_b_world = pt_on_b;
	in_contact->point_on_a_local = physics_body_world_to_local_space(in_contact->body_a, in_contact->point_on_a_world);
	in_contact->point_on_b_local = physics_body_world_to_local_space(in_contact->body_b, in_contact->point_on_b_world);	
	in_contact->separation_distance = vec3_length(vec3_sub(pt_on_a, pt_on_b));
	return false;
}

bool physics_bodies_conservative_advance(PhysicsBody* in_body_a, PhysicsBody* in_body_b, const f32 in_delta_time, PhysicsContact* in_contact)
{
	in_contact->body_a = in_body_a;
	in_contact->body_b = in_body_b;

	f32 toi = 0.f;
	i32 num_iterations = 0;

	f32 dt = in_delta_time;
	while (dt > 0.f)
	{
		const bool did_intersect = physics_bodies_intersect(in_body_a, in_body_b, in_contact);
		if (did_intersect)
		{
			in_contact->time_of_impact = toi;
			physics_body_update(in_body_a, -toi);
			physics_body_update(in_body_b, -toi);
			return true;
		}

		++num_iterations;
		if (num_iterations > 10)
		{
			break;
		}

		const Vec3 ab = vec3_normalize(vec3_sub(in_contact->point_on_b_world, in_contact->point_on_a_world));
		const f32 angular_speed_a = physics_body_get_max_linear_speed(in_body_a, in_body_a->angular_velocity, ab);
		const f32 angular_speed_b = physics_body_get_max_linear_speed(in_body_a, in_body_a->angular_velocity, vec3_scale(ab, -1.0f));
			
		const Vec3 relative_velocity = vec3_sub(in_body_a->linear_velocity, in_body_b->linear_velocity);
		const f32 ortho_speed = vec3_dot(relative_velocity, ab) + angular_speed_a + angular_speed_b;
		if (ortho_speed <= 0.f)
		{
			break;
		}

		f32 time_to_go = in_contact->separation_distance / ortho_speed;
		if (time_to_go > dt)
		{
			break;
		}

		dt -= time_to_go;
		toi += time_to_go;
		physics_body_update(in_body_a, time_to_go);
		physics_body_update(in_body_b, time_to_go);
	}

	// Unwind the clock
	physics_body_update(in_body_a, -toi);
	physics_body_update(in_body_b, -toi);
	return false;
}

// Checks for collisions over specified in_delta_time
bool physics_bodies_intersect_dt(PhysicsBody* in_body_a, PhysicsBody* in_body_b, const f32 in_delta_time, PhysicsContact* in_contact)
{
	in_contact->body_a = in_body_a;
	in_contact->body_b = in_body_b;

	const Vec3 pos_a = in_body_a->position;
	const Vec3 pos_b = in_body_b->position;

	const Vec3 vel_a = in_body_a->linear_velocity;	
	const Vec3 vel_b = in_body_b->linear_velocity;

	if (	in_body_a->shape.type == SHAPE_TYPE_SPHERE
		&&	in_body_b->shape.type == SHAPE_TYPE_SPHERE)
	{
		const SphereShape* sphere_a = &in_body_a->shape.sphere;
		const SphereShape* sphere_b = &in_body_b->shape.sphere;

		if (physics_body_intersect_sphere_sphere(
			sphere_a,
			sphere_b,
			pos_a,
			pos_b,
			vel_a,
			vel_b,
			in_delta_time, 
			&in_contact->point_on_a_world, 
			&in_contact->point_on_b_world, 
			&in_contact->time_of_impact)
		)
		{
			// Update to time of impact
			physics_body_update(in_body_a, in_contact->time_of_impact);
			physics_body_update(in_body_b, in_contact->time_of_impact);

			// Compute local space points and normal
			in_contact->point_on_a_local = physics_body_world_to_local_space(in_contact->body_a, in_contact->point_on_a_world);
			in_contact->point_on_b_local = physics_body_world_to_local_space(in_contact->body_b, in_contact->point_on_b_world);
			in_contact->normal = vec3_normalize(vec3_sub(in_body_a->position, in_body_b->position));
			
			// Roll back
			physics_body_update(in_body_a, -in_contact->time_of_impact);
			physics_body_update(in_body_b, -in_contact->time_of_impact);

			// Compute separation distance
			const Vec3 a_to_b = vec3_sub(in_body_b->position, in_body_a->position);
			in_contact->separation_distance = vec3_length(a_to_b) - (sphere_a->radius + sphere_b->radius);

			// Intersection: return true 
			return true;
		}
	}
	else
	{
		return physics_bodies_conservative_advance(in_body_a, in_body_b, in_delta_time, in_contact);
	}

	// No intersect: return false
	return false;
}

void physics_contact_resolve(PhysicsContact* in_contact)
{
	PhysicsBody* body_a = in_contact->body_a;
	PhysicsBody* body_b = in_contact->body_b;
	assert(body_a && body_b);

	const Vec3 point_on_a = in_contact->point_on_a_world;
	const Vec3 point_on_b = in_contact->point_on_b_world;
	const f32 elasticity = body_a->elasticity * body_b->elasticity;

	const f32 inverse_mass_a = body_a->inverse_mass;
	const f32 inverse_mass_b = body_b->inverse_mass;
	const f32 inverse_mass_sum = inverse_mass_a + inverse_mass_b;
	
	const Mat3 inverse_inertia_tensor_a = physics_body_get_inverse_inertia_tensor_world(body_a);
	const Mat3 inverse_inertia_tensor_b = physics_body_get_inverse_inertia_tensor_world(body_b);

	const Vec3 contact_normal = in_contact->normal;

	// Compute direction vectors from centers of mass to points on bodies
	const Vec3 ra = vec3_sub(point_on_a, physics_body_get_center_of_mass_world(body_a));
	const Vec3 rb = vec3_sub(point_on_b, physics_body_get_center_of_mass_world(body_b));

	// Get world space velocity of motion at point and rotation by combining linear and angular velocity
	const Vec3 velocity_a = vec3_add(body_a->linear_velocity, vec3_cross(body_a->angular_velocity, ra));	
	const Vec3 velocity_b = vec3_add(body_b->linear_velocity, vec3_cross(body_b->angular_velocity, rb));
	const Vec3 velocity_a_to_b = vec3_sub(velocity_a, velocity_b);

	{	// Collision Impulse

		// Use inverse inertia tensors, direction vectors, and contact normal to compute angular factors
		const Vec3 angular_ja = vec3_cross(mat3_mul_vec3(inverse_inertia_tensor_a, vec3_cross(ra, contact_normal)), ra);
		const Vec3 angular_jb = vec3_cross(mat3_mul_vec3(inverse_inertia_tensor_b, vec3_cross(rb, contact_normal)), rb);
		const f32 angular_factor = vec3_dot(vec3_add(angular_ja, angular_jb), contact_normal);

		// Calculate collision impulse magnitude
		const f32 collision_impulse_magnitude = (1.0f + elasticity) * vec3_dot(velocity_a_to_b, contact_normal) / (inverse_mass_sum + angular_factor);
		// Collision impulse is in direction of contact normal
		const Vec3 collision_impulse = vec3_scale(contact_normal, collision_impulse_magnitude);

		// Apply our collision impulses to both bodies
		physics_body_apply_impulse(body_a, vec3_negate(collision_impulse), point_on_a);
		physics_body_apply_impulse(body_b, collision_impulse, point_on_b);
	}

	{	// Friction Impulse

		// Calculate friction values
		const f32 friction_a = body_a->friction;
		const f32 friction_b = body_b->friction;
		// Total friction is product of both bodies' friction
		const f32 friction = friction_a * friction_b;

		// Scale contact normal based on similarity of contact normal to velocity_a_to_b
		const Vec3 velocity_normal = vec3_scale(contact_normal, vec3_dot(contact_normal, velocity_a_to_b));
		const Vec3 velocity_tangent = vec3_sub(velocity_a_to_b, velocity_normal);
		const Vec3 relative_velocity_tangent = vec3_normalize(velocity_tangent);

		// Compute inertia values based on inverse inertia tensors, direction to points, and relative velocity tangent
		const Vec3 inertia_a = vec3_cross(mat3_mul_vec3(inverse_inertia_tensor_a, vec3_cross(ra, relative_velocity_tangent)), ra);
		const Vec3 inertia_b = vec3_cross(mat3_mul_vec3(inverse_inertia_tensor_b, vec3_cross(rb, relative_velocity_tangent)), rb);
		// Compute final inverse inertia
		const f32 inverse_inertia = vec3_dot(vec3_add(inertia_a, inertia_b), relative_velocity_tangent);

		// Reduce mass by inverse_inertia
		const f32 reduced_mass = 1.0f / (inverse_mass_sum + inverse_inertia);
		// Use reduced mass to compute friction impulse
		const Vec3 friction_impulse = vec3_scale(velocity_tangent, reduced_mass * friction);
		physics_body_apply_impulse(body_a, vec3_negate(friction_impulse), point_on_a);
		physics_body_apply_impulse(body_b, friction_impulse, point_on_b);
	}

	// Resolve interpenetration 
	if (in_contact->time_of_impact == 0.0f)
	{
		// Move colliding objects to just outside of each other
		const f32 ta = body_a->inverse_mass / inverse_mass_sum;
		const f32 tb = body_b->inverse_mass / inverse_mass_sum;

		const Vec3 ds = vec3_sub(in_contact->point_on_b_world, in_contact->point_on_a_world);
		body_a->position = vec3_add(body_a->position, vec3_scale(ds, ta));
		body_b->position = vec3_add(body_b->position, vec3_scale(ds,-tb));
	}
}

i32 physics_contact_compare(const void* in_a, const void* in_b)
{
	const PhysicsContact* in_contact_a = (const PhysicsContact*) in_a;
	const PhysicsContact* in_contact_b = (const PhysicsContact*) in_b;

	const f32 toi_a = in_contact_a->time_of_impact;
	const f32 toi_b = in_contact_b->time_of_impact;
	return		toi_a < toi_b	? 	-1
	  		:	toi_a == toi_b	?	0
	  		: 						1;
}

typedef struct PhysicsScene
{
	sbuffer(PhysicsBody) bodies;
} PhysicsScene;

void physics_scene_init(PhysicsScene* out_physics_scene)
{
	*out_physics_scene = (PhysicsScene) {
		.bodies = NULL,
	};
}

void physics_scene_add_body(PhysicsScene* in_physics_scene, PhysicsBody* in_body)
{
	sb_push(in_physics_scene->bodies, *in_body);
}

typedef struct PseudoPhysicsBody
{
	i32 id;
	f32 value;
	bool is_min;
} PseudoPhysicsBody;

i32 pseudo_physics_body_compare(const void* a, const void* b)
{
	const PseudoPhysicsBody* pseudo_body_a = (const PseudoPhysicsBody*) a;
	const PseudoPhysicsBody* pseudo_body_b = (const PseudoPhysicsBody*) b;
	return (pseudo_body_a->value < pseudo_body_b->value) ? -1 : 1;
}

sbuffer(PseudoPhysicsBody) pseudo_physics_bodies_create_sorted(sbuffer(PhysicsBody) in_bodies, const f32 in_delta_time)
{
	// Axis we project our min and max onto 
	const Vec3 projection_axis = vec3_normalize(vec3_new(1,1,1));

	const i32 num_bodies = sb_count(in_bodies);
	const i32 num_pseudo_bodies = num_bodies * 2;

	sbuffer(PseudoPhysicsBody) out_pseudo_bodies = NULL;
	sb_reserve(out_pseudo_bodies, num_pseudo_bodies);

	for (i32 body_idx = 0; body_idx < num_bodies; ++body_idx)
	{
		const PhysicsBody* body = &in_bodies[body_idx];	
		Bounds bounds = physics_body_get_bounds(body);

		// Expand bounds by position change this timestep
		const Vec3 scaled_velocity = vec3_scale(body->linear_velocity, in_delta_time);
		const Vec3 expanded_min = vec3_add(bounds.min, scaled_velocity);
		const Vec3 expanded_max = vec3_add(bounds.max, scaled_velocity);
		bounds_expand_point(&bounds, expanded_min);
		bounds_expand_point(&bounds, expanded_max);

		// Also expand by some arbitrary extent to broader detection/rejection
		const f32 epsilon = 0.01f;
		bounds_expand_point(&bounds, vec3_add(bounds.min, vec3_scale(vec3_new(-1,-1,-1), epsilon)));
		bounds_expand_point(&bounds, vec3_add(bounds.max, vec3_scale(vec3_new( 1, 1, 1), epsilon)));

		// Create a min and max PseudoPhysicsBody by projecting the bounds min and max onto our 1D axis
		PseudoPhysicsBody new_pseudo_body_min = {
			.id = body_idx,
			.value = vec3_dot(projection_axis, bounds.min),
			.is_min = true,
		};
		sb_push(out_pseudo_bodies, new_pseudo_body_min);

		PseudoPhysicsBody new_pseudo_body_max = {
			.id = body_idx,
			.value = vec3_dot(projection_axis, bounds.max),
			.is_min = false,
		};
		sb_push(out_pseudo_bodies, new_pseudo_body_max);
	}

	// Sort pseudo bodies by their value, which is dot(axis, bounds.min) or dot(axis, bounnds.max) for a body
	qsort(out_pseudo_bodies, num_pseudo_bodies, sizeof(PseudoPhysicsBody), pseudo_physics_body_compare);

	return out_pseudo_bodies;
}

typedef struct CollisionPair
{
	i32 idx_a;
	i32 idx_b;
} CollisionPair;

bool collision_pair_equals(CollisionPair* in_lhs, CollisionPair* in_rhs)
{
	return	(		(in_lhs->idx_a == in_rhs->idx_a)
				&&	(in_lhs->idx_b == in_rhs->idx_b))
		||	(		(in_lhs->idx_a == in_rhs->idx_b)
				&&	(in_lhs->idx_b == in_rhs->idx_a));
}

sbuffer(CollisionPair) physics_scene_broad_phase(PhysicsScene* in_physics_scene, f32 in_delta_time)
{
	sbuffer(CollisionPair) out_collision_pairs = NULL;

	// Sweep and Prune 1D
	{
		// Created sorted array of pseudo bodies. Bodies are sorted by their min and max projections onto a 1D axis
		sbuffer(PseudoPhysicsBody) sorted_pseudo_bodies = pseudo_physics_bodies_create_sorted(in_physics_scene->bodies, in_delta_time);

		const i32 num_pseudo_bodies = sb_count(sorted_pseudo_bodies);

		// Build Collision Pairs from those pseudo bodies
		for (i32 i = 0; i < num_pseudo_bodies; ++i)		
		{	
			PseudoPhysicsBody* pseudo_body_a = &sorted_pseudo_bodies[i];

			// We only care about starting a search when we find a min point.
			if (!pseudo_body_a->is_min) { continue; }

			CollisionPair new_collision_pair = {
				.idx_a = pseudo_body_a->id,
			};

			for (i32 j = i+1; j < num_pseudo_bodies; ++j)		
			{
				PseudoPhysicsBody* pseudo_body_b = &sorted_pseudo_bodies[j];

				// if we hit pseudo_body_a's own max, we stop looking
				if(pseudo_body_b->id == pseudo_body_a->id) { break;}

				// We only record a collision pair if we hit the MIN point of Body B	
				if (!pseudo_body_b->is_min) { continue; }
				
				new_collision_pair.idx_b = pseudo_body_b->id;
				sb_push(out_collision_pairs, new_collision_pair);
			}
		}

		// Free pseudo bodies
		sb_free(sorted_pseudo_bodies);
	}

	// return collision pairs
	return out_collision_pairs;
}

void physics_scene_update(PhysicsScene* in_physics_scene, f32 in_delta_time)
{
	const i32 num_bodies = sb_count(in_physics_scene->bodies);

	// Acceleration due to gravity
	for (i32 body_idx = 0; body_idx < num_bodies; ++body_idx)
	{
		PhysicsBody* body = &in_physics_scene->bodies[body_idx];

		if (body->inverse_mass > 0.f)
		{
			f32 mass = 1.0f / body->inverse_mass;
			Vec3 impulse_gravity = vec3_scale(vec3_new(0.f, -10.f, 0.f), mass * in_delta_time);
			physics_body_apply_impulse_linear(body, impulse_gravity);
		}
	}

	// Broadphase
	sbuffer(CollisionPair) collision_pairs = physics_scene_broad_phase(in_physics_scene, in_delta_time);

	//printf("\033[2J\033[1;1H");
	//printf("-------------------------------------------\n");
	//printf("Num Collision Pairs: %i\n", sb_count(collision_pairs));
	//printf("Max Possible Pairs:  %i\n", (num_bodies * (num_bodies-1)) / 2);
	//printf("-------------------------------------------\n");

	// Narrowphase
	const i32 max_contacts = num_bodies * num_bodies;
	sbuffer(PhysicsContact) contacts = NULL;
	sb_reserve(contacts, max_contacts);

	for (i32 pair_idx = 0; pair_idx < sb_count(collision_pairs); ++pair_idx)
	{
		CollisionPair* collision_pair = &collision_pairs[pair_idx];
		PhysicsBody* body_a = &in_physics_scene->bodies[collision_pair->idx_a];
		PhysicsBody* body_b = &in_physics_scene->bodies[collision_pair->idx_b];

		// Skip if both bodies have zero mass
		if (body_a->inverse_mass <= 0.f && body_b->inverse_mass <= 0.f)
		{
			continue;
		}

		PhysicsContact contact = {};
		if (physics_bodies_intersect_dt(body_a, body_b, in_delta_time, &contact))
		{
			sb_push(contacts, contact);
		}
	}

	// Done with collision pairs. Free them 
	sb_free(collision_pairs);

	// Sort contacts by time of impact
	const i32 num_contacts = sb_count(contacts);
	if (num_contacts > 1)
	{
		qsort(contacts, num_contacts, sizeof(PhysicsContact), physics_contact_compare);
	}

	// peform physics body updates and contact resolution at each contact time of impact
	f32 accumulated_delta_time = 0.f;
	for (i32 contact_idx = 0; contact_idx < sb_count(contacts); ++contact_idx)
	{
		PhysicsContact* contact = &contacts[contact_idx];

		const f32 contact_delta_time = contact->time_of_impact - accumulated_delta_time;

		PhysicsBody* body_a = contact->body_a;
		PhysicsBody* body_b = contact->body_b;

		// Skip if both bodies have zero mass
		if (body_a->inverse_mass <= 0.f && body_b->inverse_mass <= 0.f)
		{
			continue;
		}

		// Position Update up to time of impact
		for (i32 body_idx = 0; body_idx < sb_count(in_physics_scene->bodies); ++body_idx)
		{
			PhysicsBody* body = &in_physics_scene->bodies[body_idx];
			physics_body_update(body, contact_delta_time);
		}

		physics_contact_resolve(contact);
		accumulated_delta_time += contact_delta_time;
	}

	// Free contacts
	sb_free(contacts);

	// Update physics bodies for any remaining delta time
	const f32 remaining_delta_time = in_delta_time - accumulated_delta_time;
	if (remaining_delta_time > 0.0f)
	{
		for (i32 body_idx = 0; body_idx < num_bodies; ++body_idx)
		{
			PhysicsBody* body = &in_physics_scene->bodies[body_idx];
			physics_body_update(body, remaining_delta_time);
		}
	}
}

