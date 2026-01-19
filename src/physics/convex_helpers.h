#pragma once

#include "basic_types.h"
#include "math/math_lib.h"

/* ------------------------------------------------ Bounds Helpers ------------------------------------------------ */

typedef struct Bounds
{
	Vec3 min;
	Vec3 max;
} Bounds;

Bounds bounds_init()
{
	return (Bounds) {
		.min = vec3_new(FLT_MAX, FLT_MAX, FLT_MAX),
		.max = vec3_new(-FLT_MAX, -FLT_MAX, -FLT_MAX),
	};
}

Vec3 bounds_get_extents(const Bounds* in_bounds)
{
	return vec3_sub(in_bounds->max, in_bounds->min);
}

Vec3 bounds_get_half_extents(const Bounds* in_bounds)
{
	return vec3_scale(bounds_get_extents(in_bounds), 0.5f);
}

bool bounds_intersect(const Bounds a, const Bounds b)
{
	if (a.max.x < b.min.x || a.max.y < b.min.y || a.max.z < b.min.z)
	{
		return false;
	}

	if (a.min.x > b.max.x || a.min.y > b.max.y || a.min.z > b.max.z)
	{
		return false;
	}

	return true;
}

void bounds_expand_point(Bounds* in_bounds, const Vec3 in_point)
{
	in_bounds->min = vec3_componentwise_min(in_bounds->min, in_point);
	in_bounds->max = vec3_componentwise_max(in_bounds->max, in_point);	
}

void bounds_expand_points(Bounds* in_bounds, const Vec3* in_points, const i32 in_num_points)
{
	for (i32 idx = 0; idx < in_num_points; ++idx)
	{
		bounds_expand_point(in_bounds, in_points[idx]);
	}	
}

void bounds_expand_bounds(Bounds* in_bounds, const Bounds* in_other_bounds)
{
	bounds_expand_point(in_bounds, in_other_bounds->min);
	bounds_expand_point(in_bounds, in_other_bounds->max);
}

/* ------------------------------------------------ Convex Helpers ------------------------------------------------ */

i32 furthest_point_in_dir(const Vec3* in_points, const i32 in_num_points, const Vec3 in_dir)
{
	assert(in_num_points > 0);

	i32 max_idx = 0;
	f32 max_dist = vec3_dot(in_dir, in_points[0]);
	for (i32 idx = 1; idx < in_num_points; ++idx)
	{
		const f32 current_dist = vec3_dot(in_dir, in_points[idx]);
		if (current_dist > max_dist)
		{
			max_idx = idx;
			max_dist = current_dist;
		}
	}
	return max_idx;
}

f32 distance_from_line(const Vec3 in_a, const Vec3 in_b, const Vec3 in_pt)
{
	// Direction of line
	const Vec3 ab = vec3_normalize(vec3_sub(in_b, in_a));
	// ray from start of line (in_a) to in_pt
	const Vec3 ray = vec3_sub(in_pt, in_a);
	// project ray onto line formed by in_a and in_b
	const Vec3 projection = vec3_scale(ab, vec3_dot(ray, ab));
	// find component of ray perpendicular to line by subtracting projection
	const Vec3 perpendicular = vec3_sub(ray, projection);
	// length of that perpendicular component is our distance from line
	return vec3_length(perpendicular);
}

Vec3 furthest_point_from_line(const Vec3* in_points, const i32 in_num_points, const Vec3 in_a, const Vec3 in_b)
{
	assert(in_num_points > 0);

	i32 max_idx = 0;
	f32 max_dist = distance_from_line(in_a, in_b, in_points[0]);
	for (i32 idx = 1; idx < in_num_points; ++idx)
	{
		const f32 current_dist = distance_from_line(in_a, in_b, in_points[idx]);
		if (current_dist > max_dist)
		{
			max_idx = idx;
			max_dist = current_dist;
		}
	}
	return in_points[max_idx];
}

f32 distance_from_triangle(const Vec3 in_a, const Vec3 in_b, const Vec3 in_c, const Vec3 in_pt)
{
	// Compute edge vectors
	const Vec3 ab = vec3_sub(in_b, in_a);
	const Vec3 ac = vec3_sub(in_c, in_a);
	// Normal formed by two edges
	const Vec3 n = vec3_normalize(vec3_cross(ab,ac));
	// Ray from first point (in_a) to in_pt
	const Vec3 ray = vec3_sub(in_pt, in_a);
	// compute distance by projecting ray onto n
	const f32 dist = vec3_dot(ray, n);
	return dist;
}

Vec3 furthest_point_from_triangle(const Vec3* in_points, const i32 in_num_points, const Vec3 in_a, const Vec3 in_b, const Vec3 in_c)
{
	assert(in_num_points > 0);

	i32 max_idx = 0;
	const f32 max_dist = distance_from_triangle(in_a, in_b, in_c, in_points[0]);
	f32 max_dist_squared = max_dist * max_dist;
	for (i32 idx = 1; idx < in_num_points; ++idx)
	{
		const f32 current_dist = distance_from_triangle(in_a, in_b, in_c, in_points[idx]);
		const f32 current_dist_squared = current_dist * current_dist;
		if (current_dist_squared > max_dist_squared)
		{
			max_idx = idx;
			max_dist_squared = current_dist_squared;
		}
	}
	return in_points[max_idx];
}

/* ------------------------------------------------ Convex Hull ------------------------------------------------ */

typedef struct ConvexTri 
{
	i32 a;
	i32 b;
	i32 c;
} ConvexTri;

typedef struct ConvexHull
{
	sbuffer(Vec3) points;
	sbuffer(ConvexTri) tris;
} ConvexHull;

ConvexHull convex_hull_build_tetrahedron(const Vec3* in_points, const i32 in_num_points)
{
	Vec3 points[4];

	// Point 0: furthest point in arbitrary direction
	const i32 idx_0 = furthest_point_in_dir(in_points, in_num_points, vec3_new(1,0,0));
	points[0] = in_points[idx_0];

	// Point 1: furthest point in direction opposite point[0]
	const i32 idx_1 = furthest_point_in_dir(in_points, in_num_points, vec3_scale(points[0], -1.0f));
	points[1] = in_points[idx_1];

	// Point 2: furthest point from line formed by previous points 
	points[2] = furthest_point_from_line(in_points, in_num_points, points[0], points[1]);

	// Point 3: furthest point from triangle formed by previous points
	points[3] = furthest_point_from_triangle(in_points, in_num_points, points[0], points[1], points[2]);

	// Ensure CCW
	if (distance_from_triangle(points[0], points[1], points[2], points[3]) > 0.f)
	{
		SWAP(Vec3, points[0], points[1]);
	}

	// Build up convex hull
	ConvexHull out_hull = {
		.points = NULL,
		.tris = NULL,
	};

	// Add points
	sb_push(out_hull.points, points[0]);
	sb_push(out_hull.points, points[1]);
	sb_push(out_hull.points, points[2]);
	sb_push(out_hull.points, points[3]);

	// Add tris
	sb_push(out_hull.tris, ((ConvexTri) {
		.a = 0,
		.b = 1,
		.c = 2,
	}));
	sb_push(out_hull.tris, ((ConvexTri) {
		.a = 0,
		.b = 2,
		.c = 3,
	}));
	sb_push(out_hull.tris, ((ConvexTri) {
		.a = 2,
		.b = 1,
		.c = 3,
	}));
	sb_push(out_hull.tris, ((ConvexTri) {
		.a = 1,
		.b = 0,
		.c = 3,
	}));

	return out_hull;
}

void convex_hull_remove_internal_points(ConvexHull* in_convex_hull, sbuffer(Vec3)* in_points)
{
	// Remove internal points
	for (i32 i = 0; i < sb_count(*in_points); ++i)
	{
		const Vec3 point = (*in_points)[i];
		bool is_external = false;
		for (int t = 0; t < sb_count(in_convex_hull->tris); ++t)
		{
			const ConvexTri* tri = &in_convex_hull->tris[t];
			const Vec3 a = in_convex_hull->points[tri->a];
			const Vec3 b = in_convex_hull->points[tri->b];
			const Vec3 c = in_convex_hull->points[tri->c];

			if(distance_from_triangle(a,b,c, point) > 0.f)
			{
				is_external = true;
				break;
			}
		}

		if (!is_external)
		{
			sb_del(*in_points, i);
			i -= 1;
		}
	}

	// Remove points too close too hull points
	for (i32 i = 0; i < sb_count(*in_points); ++i)
	{
		const Vec3 point = (*in_points)[i];
		bool is_too_close = false;
		for (i32 hull_pt_idx = 0; hull_pt_idx < sb_count(in_convex_hull->points); ++hull_pt_idx)
		{
			const Vec3 hull_point = in_convex_hull->points[hull_pt_idx];
			const Vec3 ray = vec3_sub(hull_point, point);
			if (vec3_length_squared(ray)  < (0.01f * 0.01f))
			{
				is_too_close = true;
				break;
			}
		}

		if (is_too_close)
		{	
			sb_del(*in_points, i);
			i -= 1;
		}
	}
}

typedef struct ConvexEdge
{
	i32 a;
	i32 b;
} ConvexEdge;

bool convex_edge_equals(const ConvexEdge lhs, const ConvexEdge rhs)
{
	return	(lhs.a == rhs.a && lhs.b == rhs.b) 
		||	(lhs.a == rhs.b && lhs.b == rhs.a);
}

bool is_edge_unique(sbuffer(ConvexTri) in_tris, sbuffer(i32) in_facing_tri_idices, i32 in_ignore_tri, const ConvexEdge in_edge)
{
	for (i32 i = 0; i < sb_count(in_facing_tri_idices); ++i)
	{
		const i32 tri_idx = in_facing_tri_idices[i];
		if (tri_idx == in_ignore_tri)
		{
			continue;
		}

		const ConvexTri tri = in_tris[tri_idx];
		ConvexEdge edges[3] = {
			{
				.a = tri.a,
				.b = tri.b,
			},
			{
				.a = tri.b,
				.b = tri.c,
			},
			{
				.a = tri.c,
				.b = tri.a,
			},
		};

		for (i32 e = 0; e < ARRAY_COUNT(edges); ++e)
		{
			if (convex_edge_equals(edges[e], in_edge))
			{
				return false;
			}
		}
	}

	return true;
}

void convex_hull_add_point(ConvexHull* in_convex_hull, const Vec3 in_point)
{
	// Find all triangles that face this point
	sbuffer(i32) facing_tri_indices = NULL;
	for (i32 i = sb_count(in_convex_hull->tris); i >= 0; --i)
	{
		const ConvexTri tri = in_convex_hull->tris[i];
		const Vec3 a = in_convex_hull->points[tri.a];
		const Vec3 b = in_convex_hull->points[tri.b];
		const Vec3 c = in_convex_hull->points[tri.c];

		if (distance_from_triangle(a,b,c,in_point) > 0.f)
		{
			sb_push(facing_tri_indices, i);
		}
	}

	// Find all edges unique to this tri. These will form the new triangles
	sbuffer(ConvexEdge) unique_edges =  NULL;	
	for (i32 i = 0; i < sb_count(facing_tri_indices); ++i)
	{
		const i32 tri_idx = facing_tri_indices[i];
		const ConvexTri tri = in_convex_hull->tris[tri_idx];

		ConvexEdge edges[3] = {
			{
				.a = tri.a,
				.b = tri.b,
			},
			{
				.a = tri.b,
				.b = tri.c,
			},
			{
				.a = tri.c,
				.b = tri.a,
			},
		};

		for (i32 e = 0; e < ARRAY_COUNT(edges); ++e)
		{
			if (is_edge_unique(in_convex_hull->tris, facing_tri_indices, tri_idx, edges[e]))
			{
				sb_push(unique_edges, edges[e]);
			}
		}
	}

	// remove old facing tri indices
	for (i32 i = 0; i < sb_count(facing_tri_indices); ++i)
	{
		sb_del(in_convex_hull->tris, facing_tri_indices[i]);
	}

	// add the new point
	sb_push(in_convex_hull->points, in_point);
	i32 new_point_idx = sb_count(in_convex_hull->points) - 1;

	// Add hull triangles for each unique edge
	for (i32 i = 0; i < sb_count(unique_edges); ++i)
	{
		const ConvexEdge edge = unique_edges[i];
		sb_push(in_convex_hull->tris, ((ConvexTri) {
			.a = edge.a,
			.b = edge.b,
			.c = new_point_idx,
		}));
	}

	sb_free(facing_tri_indices);
	sb_free(unique_edges);
}

void convex_hull_remove_unreferenced_points(ConvexHull* in_convex_hull)
{
	for (i32 point_idx = 0; point_idx < sb_count(in_convex_hull->points); ++point_idx)
	{
		bool is_used = false;
		for (i32 tri_idx = 0; tri_idx < sb_count(in_convex_hull->tris); ++tri_idx)
		{
			ConvexTri tri = in_convex_hull->tris[tri_idx];
			if (tri.a == point_idx || tri.b == point_idx || tri.c == point_idx)
			{
				is_used = true;
				break;
			}
		}

		if (is_used)
		{
			continue;
		}

		for (i32 tri_idx = 0; tri_idx < sb_count(in_convex_hull->tris); ++tri_idx)
		{
			ConvexTri* tri = &in_convex_hull->tris[tri_idx];
			if (tri->a > point_idx)
			{
				tri->a -= 1;
			}

			if (tri->b > point_idx)
			{
				tri->b -= 1;
			}

			if (tri->c > point_idx)
			{
				tri->c -= 1;
			}	
		}

		sb_del(in_convex_hull->points, point_idx);
		point_idx -= 1;
	}
}

void convex_hull_expand(ConvexHull* in_convex_hull, const Vec3* in_points, const i32 in_num_points)
{
	sbuffer(Vec3) external_points = NULL;
	sb_append_array(external_points, in_points, in_num_points);

	// Remove any internal points
	convex_hull_remove_internal_points(in_convex_hull, &external_points);

	while(sb_count(external_points) > 0)
	{
		i32	point_idx = furthest_point_in_dir(external_points, sb_count(external_points), external_points[0]);
		const Vec3 point = external_points[point_idx];

		// Remove this element
		sb_del(external_points, point_idx);

		// Add this element to our hull
		convex_hull_add_point(in_convex_hull, point);

		// Re-run internal point removal
		convex_hull_remove_internal_points(in_convex_hull, &external_points);
	}

	// Finall remove any points that are no longer referenced
	convex_hull_remove_unreferenced_points(in_convex_hull);

	sb_free(external_points);
}

ConvexHull convex_hull_create(const Vec3* in_points, const i32 in_num_points)
{
	ConvexHull convex_hull = convex_hull_build_tetrahedron(in_points, in_num_points);	

	convex_hull_expand(&convex_hull, in_points, in_num_points);

	return convex_hull;
}

bool convex_hull_is_point_external(const ConvexHull* in_convex_hull, const Vec3 in_point)
{	
	bool is_external = false;
	for (i32 t = 0; t < sb_count(in_convex_hull->tris); ++t)
	{
		const ConvexTri tri = in_convex_hull->tris[t];
		const Vec3 a = in_convex_hull->points[tri.a];
		const Vec3 b = in_convex_hull->points[tri.b];	
		const Vec3 c = in_convex_hull->points[tri.c];
		if (distance_from_triangle(a, b, c, in_point) > 0.f)
		{
			is_external = true;
			break;
		}
	}

	return is_external;
}

Vec3 convex_hull_calculate_center_of_mass_monte_carlo(const ConvexHull* in_convex_hull)
{
	const i32 num_samples = 10000;

	Bounds bounds = bounds_init();
	bounds_expand_points(&bounds, in_convex_hull->points, sb_count(in_convex_hull->points));

	Vec3 bounds_dimensions = bounds_get_extents(&bounds);

	Vec3 center_of_mass = vec3_zero;
	i32 sample_count = 0;

	for (i32 i = 0; i < num_samples; ++i)
	{
		Vec3 point = vec3_new(
			bounds.min.x + rand_f32(0.f, bounds_dimensions.x),
			bounds.min.y + rand_f32(0.f, bounds_dimensions.y),
			bounds.min.z + rand_f32(0.f, bounds_dimensions.z)
		);

		if (convex_hull_is_point_external(in_convex_hull, point))
		{
			continue;
		}

		center_of_mass = vec3_add(center_of_mass, point);
		sample_count += 1;
	}

	center_of_mass = vec3_scale(center_of_mass, 1.0 / (f32) sample_count);
	return center_of_mass;
}

Mat3 convex_hull_calculate_inertia_tensor_monte_carlo(const ConvexHull* in_convex_hull)
{
	const i32 num_samples = 10000;

	Bounds bounds = bounds_init();
	bounds_expand_points(&bounds, in_convex_hull->points, sb_count(in_convex_hull->points));

	Vec3 bounds_dimensions = bounds_get_extents(&bounds);

	Mat3 inertia_tensor = mat3_zero;
	i32 sample_count = 0;

	for (i32 i = 0; i < num_samples; ++i)
	{
		Vec3 point = vec3_new(
			bounds.min.x + rand_f32(0.f, bounds_dimensions.x),
			bounds.min.y + rand_f32(0.f, bounds_dimensions.y),
			bounds.min.z + rand_f32(0.f, bounds_dimensions.z)
		);

		if (convex_hull_is_point_external(in_convex_hull, point))
		{
			continue;
		}

		inertia_tensor.columns[0].x += point.y * point.y + point.z * point.z;
		inertia_tensor.columns[1].y += point.z * point.z + point.x * point.x;
		inertia_tensor.columns[2].z += point.x * point.x + point.y * point.y;

		inertia_tensor.columns[0].y += -1.0f * point.x * point.y;
		inertia_tensor.columns[0].z += -1.0f * point.x * point.z;
		inertia_tensor.columns[1].z += -1.0f * point.y * point.z;

		inertia_tensor.columns[1].x += -1.0f * point.x * point.y;
		inertia_tensor.columns[2].x += -1.0f * point.x * point.z;
		inertia_tensor.columns[2].y += -1.0f * point.y * point.z;

		sample_count += 1;
	}

	inertia_tensor = mat3_mul_f32(inertia_tensor, 1.0 / (f32) sample_count);
	return inertia_tensor;
}

Vec3 convex_hull_calculate_center_of_mass(const ConvexHull* in_convex_hull)
{
	const i32 num_samples = 100;

	Bounds bounds = bounds_init();
	bounds_expand_points(&bounds, in_convex_hull->points, sb_count(in_convex_hull->points));

	Vec3 bounds_dimensions = bounds_get_extents(&bounds);

	const f32 dx = bounds_dimensions.x / num_samples;
	const f32 dy = bounds_dimensions.y / num_samples;
	const f32 dz = bounds_dimensions.z / num_samples;

	Vec3 center_of_mass = vec3_zero;
	i32 sample_count = 0;
	for (f32 x = bounds.min.x; x < bounds.max.x; x += dx)
	{
		for (f32 y = bounds.min.y; y < bounds.max.y; y += dy)
		{
			for (f32 z = bounds.min.z; z < bounds.max.z; z += dz)
			{
				const Vec3 point = vec3_new(x,y,z);
				if (convex_hull_is_point_external(in_convex_hull, point))
				{
					continue;
				}

				center_of_mass = vec3_add(center_of_mass, point);
				sample_count += 1;
			}
		}
	}

	center_of_mass = vec3_scale(center_of_mass, 1.0 / (f32) sample_count);
	return center_of_mass;
}

Mat3 convex_hull_calculate_inertia_tensor(const ConvexHull* in_convex_hull)
{
	const i32 num_samples = 100;

	Bounds bounds = bounds_init();
	bounds_expand_points(&bounds, in_convex_hull->points, sb_count(in_convex_hull->points));

	Vec3 bounds_dimensions = bounds_get_extents(&bounds);

	const f32 dx = bounds_dimensions.x / num_samples;
	const f32 dy = bounds_dimensions.y / num_samples;
	const f32 dz = bounds_dimensions.z / num_samples;

	Mat3 inertia_tensor = mat3_zero;
	i32 sample_count = 0;
	for (f32 x = bounds.min.x; x < bounds.max.x; x += dx)
	{
		for (f32 y = bounds.min.y; y < bounds.max.y; y += dy)
		{
			for (f32 z = bounds.min.z; z < bounds.max.z; z += dz)
			{
				const Vec3 point = vec3_new(x,y,z);
				if (convex_hull_is_point_external(in_convex_hull, point))
				{
					continue;
				}

				inertia_tensor.columns[0].x += point.y * point.y + point.z * point.z;
				inertia_tensor.columns[1].y += point.z * point.z + point.x * point.x;
				inertia_tensor.columns[2].z += point.x * point.x + point.y * point.y;

				inertia_tensor.columns[0].y += -1.0f * point.x * point.y;
				inertia_tensor.columns[0].z += -1.0f * point.x * point.z;
				inertia_tensor.columns[1].z += -1.0f * point.y * point.z;

				inertia_tensor.columns[1].x += -1.0f * point.x * point.y;
				inertia_tensor.columns[2].x += -1.0f * point.x * point.z;
				inertia_tensor.columns[2].y += -1.0f * point.y * point.z;

				sample_count += 1;
			}
		}
	}

	inertia_tensor = mat3_mul_f32(inertia_tensor, 1.0 / (f32) sample_count);
	return inertia_tensor;
}

void convex_hull_destroy(ConvexHull* in_convex_hull)
{
	sb_free(in_convex_hull->points);
	sb_free(in_convex_hull->tris);
}

/* ------------------------------------------------ Signed Volume Helpers ------------------------------------------------ */

i32 compare_signs(f32 a, f32 b)
{
	if (a > 0.0f && b > 0.0f)
	{
		return 1;
	}

	if (a < 0.0f && b < 0.0f)
	{
		return 1;
	}

	return 0;
}

Vec2 signed_volume_1d(const Vec3 s1, const Vec3 s2)
{
	const Vec3 ab = vec3_sub(s2, s1);
	const Vec3 ap = vec3_sub(vec3_zero, s1);
	const Vec3 p0 = vec3_add(s1, vec3_scale(ab, vec3_dot(ab, ap) / vec3_length_squared(ab)));

	i32 idx = 0;
	f32 mu_max = 0;
	for (i32 i = 0; i < 3; ++i)
	{
		f32 mu = s2.v[i] - s1.v[i];
		if (mu * mu > mu_max * mu_max)
		{
			idx = i;
			mu_max = mu;
		}
	}

	const f32 a = s1.v[idx];
	const f32 b = s2.v[idx];
	const f32 p = p0.v[idx];

	const f32 c1 = p - a;
	const f32 c2 = b - p;

	if ((p > a && p < b) || (p > b && p < a))
	{
		return vec2_new(c2 / mu_max, c1 / mu_max);
	}

	if ((a <= b && p <= a) || (a >= b && p >= a))
	{
		return vec2_new(1.0f, 0.0f);
	}

	return vec2_new(0.0f, 1.0f);
}

Vec3 signed_volume_2d(const Vec3 s1, const Vec3 s2, const Vec3 s3)
{
	const Vec3 normal = vec3_cross(vec3_sub(s2, s1), vec3_sub(s3, s1));
	const Vec3 p0 = vec3_scale(normal, vec3_dot(s1, normal) / vec3_length_squared(normal));

	// Find axis with greatest projected area
	i32 idx = 0;
	f32 area_max = 0.f;
	f32 area_max_squared = 0.f;
	for (i32 i = 0; i < 3; ++i)
	{
		const i32 j = (i + 1) % 3;		
		const i32 k = (i + 2) % 3;

		const Vec2 a = vec2_new(s1.v[j], s1.v[k]);
		const Vec2 b = vec2_new(s2.v[j], s2.v[k]);
		const Vec2 c = vec2_new(s3.v[j], s3.v[k]);
		const Vec2 ab = vec2_sub(b,a);
		const Vec2 ac = vec2_sub(c,a);

		const f32 area = ab.x * ac.y - ab.y * ac.x;
		const f32 area_squared = area * area;	
		if (area_squared > area_max_squared)
		{
			idx = i;
			area_max = area;
			area_max_squared = area_squared;
		}
	}

	// Project onto the appropriate axis
	i32 x = (idx + 1) % 3;
	i32 y = (idx + 2) % 3;
	Vec2 s[3] = {
		vec2_new(s1.v[x], s1.v[y]),
		vec2_new(s2.v[x], s2.v[y]),	
		vec2_new(s3.v[x], s3.v[y]),
	};
	Vec2 p = vec2_new(p0.v[x], p0.v[y]);

	// Get the sub-areas of the triangles formed from the projected origin and the edges
	Vec3 areas;
	for (i32 i = 0; i < 3; ++i)
	{
		const i32 j = (i + 1) % 3;		
		const i32 k = (i + 2) % 3;
		
		const Vec2 a = p;
		const Vec2 b = s[j];
		const Vec2 c = s[k];
		const Vec2 ab = vec2_sub(b,a);
		const Vec2 ac = vec2_sub(c,a);

		areas.v[i] = ab.x * ac.y - ab.y * ac.x;
	}

	// If the projected origin is inside the triangle, return barycentric points
	if (
		compare_signs(area_max, areas.x) > 0
	&&	compare_signs(area_max, areas.y) > 0	
	&&	compare_signs(area_max, areas.z) > 0
	)
	{
		return vec3_scale(areas, 1 / area_max);
	}

	// If we make it here, we need to project onto edges and determine the closest point
	f32 dist = 1e10;
	Vec3 lambdas = vec3_new(1,0,0);
	for (i32 i = 0; i < 3; ++i)
	{
		const i32 k = (i + 1) % 3;		
		const i32 l = (i + 2) % 3;

		const Vec3 edges_pts[3] = {
			s1,
			s2,
			s3
		};

		Vec2 lambda_edge = signed_volume_1d(edges_pts[k], edges_pts[l]);
		Vec3 pt = vec3_add(vec3_scale(edges_pts[k], lambda_edge.x), vec3_scale(edges_pts[l], lambda_edge.y));
		f32 pt_length_squared = vec3_length_squared(pt);
		if (pt_length_squared < dist)
		{
			dist = pt_length_squared;
			lambdas.v[i] = 0;
			lambdas.v[k] = lambda_edge.x;
			lambdas.v[l] = lambda_edge.y;
		}
	}

	return lambdas;
}


Vec4 signed_volume_3d(const Vec3 s1, const Vec3 s2, const Vec3 s3, const Vec3 s4)
{
	Mat4 m = {
		.columns = {
			vec4_new(s1.x, s1.y, s1.z, 1.0f),
			vec4_new(s2.x, s2.y, s2.z, 1.0f),
			vec4_new(s3.x, s3.y, s3.z, 1.0f),
			vec4_new(s4.x, s4.y, s4.z, 1.0f),
		}
	};

	//FCS TODO: Verify this
	Vec4 c4 = vec4_new(
		mat4_cofactor(m, 0, 3),
		mat4_cofactor(m, 1, 3),
		mat4_cofactor(m, 2, 3),
		mat4_cofactor(m, 3, 3)
	);

	const f32 det_m = c4.x + c4.y + c4.z + c4.w;

	// If the barycentric coordinates put the origin inside the simplex, then return them
	if (compare_signs(det_m , c4.x) > 0
	&&	compare_signs(det_m , c4.y) > 0
	&&	compare_signs(det_m , c4.z) > 0
	&&	compare_signs(det_m , c4.w) > 0)
	{
		Vec4 lambdas = vec4_scale(c4, 1.0f / det_m);
		return lambdas;
	}

	// If we get here then we need to project the origin onto the faces and determine the closest one
	Vec4 lambdas;
	float dist = 1e10;
	for (i32 i = 0; i < 4; ++i)
	{
		const i32 j = (i + 1) % 4;
		const i32 k = (i + 2) % 4;

		const Vec3 face_pts[4] = {
			s1,
			s2,
			s3,
			s4
		};

		const Vec3 lambdas_face = signed_volume_2d(face_pts[i], face_pts[j], face_pts[k]);
		Vec3 pt = vec3_zero;
	  	pt = vec3_add(pt, vec3_scale(face_pts[i], lambdas_face.x));	
	  	pt = vec3_add(pt, vec3_scale(face_pts[j], lambdas_face.y));
	  	pt = vec3_add(pt, vec3_scale(face_pts[k], lambdas_face.z));
		const f32 pt_length_squared = vec3_length_squared(pt);
		if (pt_length_squared < dist)
		{
			dist = pt_length_squared;
			lambdas = vec4_zero;
			lambdas.v[i] = lambdas_face.v[0];
			lambdas.v[j] = lambdas_face.v[1];
			lambdas.v[k] = lambdas_face.v[2];
		}	
	}
	return lambdas;
}

void signed_volume_projection_test_case_pts(const Vec3 pts[4])
{	
	printf("-----------------------------------------------------------\n");

	Vec4 lambdas = signed_volume_3d(pts[0], pts[1], pts[2], pts[3]);

	Vec3 v = vec3_zero;
	for (i32 i = 0; i < 4; ++i)
	{	
		v = vec3_add(v, vec3_scale(pts[i], lambdas.v[i]));
	}
	printf("Lambdas: %.3f %.3f %.3f %.3f\n", lambdas.x, lambdas.y, lambdas.z, lambdas.w);
	printf("v: %.3f %.3f %.3f\n", v.x, v.y, v.z);
}

void signed_volume_projection_test_case_offset(Vec3 in_offset)
{
	const Vec3 pts[4] =
	{
		vec3_add(vec3_new(0,0,0), in_offset),
		vec3_add(vec3_new(1,0,0), in_offset),
		vec3_add(vec3_new(0,1,0), in_offset),
		vec3_add(vec3_new(0,0,1), in_offset),
	};

	signed_volume_projection_test_case_pts(pts);
}

void signed_volume_projection_test()
{
	signed_volume_projection_test_case_offset(vec3_new(1,1,1));
	signed_volume_projection_test_case_offset(vec3_scale(vec3_new(-1,-1,-1), 0.25f));
	signed_volume_projection_test_case_offset(vec3_new(-1,-1,-1));
	signed_volume_projection_test_case_offset(vec3_new(1,1,-0.5));

	const Vec3 pts[4] =
	{
		vec3_new(51.1996613f, 26.1989613f, 1.91339576f),
		vec3_new(-51.0567360f, -26.0565681f, -0.436143428f),
		vec3_new(50.8978920f, -24.1035538f, -1.04042661f),
		vec3_new(-49.1021080f, 25.8964462f, -1.04042661f),
	};
	signed_volume_projection_test_case_pts(pts);

	exit(0);	
}

/* ------------------------------------------------ Minkowski Helpers ------------------------------------------------ */

typedef struct MinkowskiPoint
{
	Vec3 xyz;
	Vec3 pt_a;
	Vec3 pt_b;
} MinkowskiPoint;

bool minkowski_point_equals(const MinkowskiPoint* a, const MinkowskiPoint* b)
{
	return 	vec3_equals(a->xyz, b->xyz)
		&&	vec3_equals(a->pt_a, b->pt_a)
		&&	vec3_equals(a->pt_b, b->pt_b);
}

bool simplex_signed_volumes(MinkowskiPoint* in_points, const i32 in_num_points, Vec3* out_new_dir, Vec4* out_lambdas)
{
	assert(in_points && in_num_points > 0);

	*out_lambdas = vec4_zero;

	const f32 epsilon_squared = 0.0001f * 0.0001f;

	bool does_intersect = false;
	switch (in_num_points)
	{
		case 2:
		{
			const Vec2 lambdas = signed_volume_1d(in_points[0].xyz, in_points[1].xyz);
			Vec3 v = vec3_zero;
			for (i32 i = 0; i < 2; ++i)
			{
				v = vec3_add(v, vec3_scale(in_points[i].xyz, lambdas.v[i]));
			}
			does_intersect = vec3_length_squared(v) < epsilon_squared;
			*out_new_dir = vec3_scale(v, -1.0f);
			*out_lambdas = vec4_new(lambdas.x, lambdas.y, 0.f, 0.f);
			break;
		}
		case 3:
		{
			const Vec3 lambdas = signed_volume_2d(in_points[0].xyz, in_points[1].xyz, in_points[2].xyz);
			Vec3 v = vec3_zero;
			for (i32 i = 0; i < 3; ++i)
			{
				v = vec3_add(v, vec3_scale(in_points[i].xyz, lambdas.v[i]));
			}
			does_intersect = vec3_length_squared(v) < epsilon_squared;
			*out_new_dir = vec3_scale(v, -1.0f);
			*out_lambdas = vec4_new(lambdas.x, lambdas.y, lambdas.z, 0.f);
			break;
		}
		case 4:
		{
			const Vec4 lambdas = signed_volume_3d(in_points[0].xyz, in_points[1].xyz, in_points[2].xyz, in_points[3].xyz);
			Vec3 v = vec3_zero;
			for (i32 i = 0; i < 4; ++i)
			{
				v = vec3_add(v, vec3_scale(in_points[i].xyz, lambdas.v[i]));
			}
			does_intersect = vec3_length_squared(v) < epsilon_squared;
			*out_new_dir = vec3_scale(v, -1.0f);
			*out_lambdas = vec4_new(lambdas.x, lambdas.y, lambdas.z, lambdas.w);
			break;
		}
	}

	return does_intersect;
}

// FCS TODO: passing count in to this so we don't check uninitialized simplex points
bool simplex_has_point(const MinkowskiPoint* in_simplex_points, const i32 in_num_simplex_points, const MinkowskiPoint* in_new_point)
{
	const f32 precision = 1e-6f;
	const f32 precision_squared = precision * precision;
	for (i32 i = 0; i < in_num_simplex_points; ++i)
	{
		const MinkowskiPoint* current_point = &in_simplex_points[i];
		const Vec3 delta = vec3_sub(current_point->xyz, in_new_point->xyz);
		if (vec3_length_squared(delta) < precision_squared)
		{
			return true;
		}
	}
	return false;
}

void sort_valids(MinkowskiPoint in_out_simplex_points[4], Vec4* in_out_lambdas)
{
	bool valids[4];
	for (i32 i = 0; i < 4; ++i)
	{
		valids[i] = true;
		if (in_out_lambdas->v[i] == 0.0f)
		{
			valids[i] = false;
		}
	}


	Vec4 valid_lambdas = vec4_zero;
	i32 valid_count = 0;
	MinkowskiPoint valid_points[4];
	memset(valid_points, 0, sizeof(MinkowskiPoint) * 4);

	for (i32 i = 0; i < 4; ++i)
	{
		if (valids[i])
		{
			valid_points[valid_count] = in_out_simplex_points[i];	
			valid_lambdas.v[valid_count] = in_out_lambdas->v[i];
			++valid_count;
		}
	}

	for (i32 i = 0; i < 4; ++i)
	{
		in_out_simplex_points[i] = valid_points[i];
		in_out_lambdas->v[i] = valid_lambdas.v[i];
	}
}

i32 num_valids(const Vec4 in_lambdas)
{
	i32 num = 0;
	for (i32 i = 0; i < 4; ++i)
	{
		if (in_lambdas.v[i] != 0.0f)
		{
			++num;
		}
	}
	return num;
}

/* ------------------------------------------------ EPA Helpers ------------------------------------------------ */

Vec3 barycentric_coordinates(Vec3 s1, Vec3 s2, Vec3 s3, const Vec3 in_pt)
{
	s1 = vec3_sub(s1,in_pt);
	s2 = vec3_sub(s2,in_pt);
	s3 = vec3_sub(s3,in_pt);

	const Vec3 normal = vec3_cross(vec3_sub(s2,s1), vec3_sub(s3,s1));
	const Vec3 p0 = vec3_scale(normal, vec3_dot(s1,normal) / vec3_length_squared(normal));
	
	// Find axis with greatest projected area
	i32 idx = 0;
	f32 area_max = 0;
	for (i32 i = 0; i < 3; ++i)
	{
		i32 j = (i + 1) % 3;
		i32 k = (i + 2) % 3;
		const Vec2 a = vec2_new(s1.v[j], s1.v[k]);
		const Vec2 b = vec2_new(s2.v[j], s2.v[k]);
		const Vec2 c = vec2_new(s3.v[j], s3.v[k]);
		const Vec2 ab = vec2_sub(b,a);
		const Vec2 ac = vec2_sub(c,a);

		f32 area = ab.x * ac.y - ab.y * ac.x;
		if (area * area > area_max * area_max)
		{
			idx = i;
			area_max = area;
		}
	}

	// Project onto appropriate axis
	const i32 x = (idx + 1) % 3;
	const i32 y = (idx + 2) % 3;
	const Vec2 s[3] =
	{
		vec2_new(s1.v[x], s1.v[y]),
		vec2_new(s2.v[x], s2.v[y]),
		vec2_new(s3.v[x], s3.v[y]),
	};
	const Vec2 p = vec2_new(p0.v[x], p0.v[y]);

	// Get the sub-areas of the triangles formed from the projected origin and the edges
	Vec3 areas;
	for (i32 i = 0; i < 3; ++i)
	{	
		i32 j = (i + 1) % 3;
		i32 k = (i + 2) % 3;

		const Vec2 a = p;
		const Vec2 b = s[j];
		const Vec2 c = s[k];
		const Vec2 ab = vec2_sub(b,a);
		const Vec2 ac = vec2_sub(c,a);

		areas.v[i] = ab.x * ac.y - ab.y * ac.x;
	}

	Vec3 lambdas = vec3_scale(areas, 1.0f / area_max);
	if (!vec3_is_valid(lambdas))
	{
		lambdas = vec3_new(1,0,0);
	}
	return lambdas;	
}

Vec3 triangle_normal_direction(const ConvexTri in_tri, const MinkowskiPoint* in_points, const i32 in_num_points)
{
	assert(in_tri.a < in_num_points);
	assert(in_tri.b < in_num_points);
	assert(in_tri.c < in_num_points);

	const Vec3 a = in_points[in_tri.a].xyz;
	const Vec3 b = in_points[in_tri.b].xyz;
	const Vec3 c = in_points[in_tri.c].xyz;
	const Vec3 ab = vec3_sub(b,a);
	const Vec3 ac = vec3_sub(c,a);

	const Vec3 normal = vec3_normalize(vec3_cross(ab,ac));
	return normal;
}

f32 signed_distance_to_triangle(const ConvexTri in_tri, const Vec3 in_test_point, const MinkowskiPoint* in_points, const i32 in_num_points)
{
	const Vec3 normal = triangle_normal_direction(in_tri, in_points, in_num_points);
	const Vec3 a = in_points[in_tri.a].xyz;
	const Vec3 a_to_test_point = vec3_sub(in_test_point, a);
	const f32 dist = vec3_dot(normal, a_to_test_point);
	return dist;
}

i32 closest_triangle(const ConvexTri* in_tris, const i32 in_num_tris, const MinkowskiPoint* in_points, const i32 in_num_points)
{
	i32 idx = -1;
	f32 min_dist_squared = 1e10;
	for (i32 i = 0; i < in_num_tris; ++i)
	{
		const ConvexTri tri = in_tris[i];
		const f32 dist = signed_distance_to_triangle(tri, vec3_zero, in_points, in_num_points);
		const f32 dist_squared = dist * dist;
		if (dist_squared < min_dist_squared)
		{
			idx = i;	
			min_dist_squared = dist_squared;
		}
	}
	return idx;
}

bool triangle_has_point(const Vec3 w, const ConvexTri* in_tris, const i32 in_num_tris, const MinkowskiPoint* in_points, const i32 in_num_points)
{
	const f32 epsilons = 0.001f * 0.001f;

	for (i32 i = 0; i < in_num_tris; ++i)
	{
		const ConvexTri tri = in_tris[i];

		if (vec3_length_squared(vec3_sub(w, in_points[tri.a].xyz)) < epsilons)
		{
			return true;
		}

		if (vec3_length_squared(vec3_sub(w, in_points[tri.b].xyz)) < epsilons)
		{
			return true;
		}

		if (vec3_length_squared(vec3_sub(w, in_points[tri.c].xyz)) < epsilons)
		{
			return true;
		}
	}
	return false;
}

i32 remove_triangles_facing_point(const Vec3 pt, sbuffer(ConvexTri)* in_out_tris, const MinkowskiPoint* in_points, const i32 in_num_points)
{
	i32 num_removed = 0;

	for (i32 i = 0; i < sb_count(*in_out_tris); ++i)
	{	
		const ConvexTri tri = (*in_out_tris)[i];
		if (signed_distance_to_triangle(tri, pt, in_points, in_num_points) > 0.f)
		{
			sb_del(*in_out_tris, i);	
			i -= 1;
			num_removed += 1;
		}
	}
	return num_removed;
}

void find_dangling_edges(const ConvexTri* in_tris, const i32 in_num_tris, sbuffer(ConvexEdge)* out_dangling_edges)
{
	assert(out_dangling_edges != NULL);
	
	// Reset out dangling edges
	sb_free(*out_dangling_edges);

	sbuffer(ConvexEdge) out_edges = NULL;

	for (i32 idx_1 = 0; idx_1 < in_num_tris; ++idx_1)
	{
		const ConvexTri tri_1 = in_tris[idx_1];
		ConvexEdge edges_1[3] =
		{
			{ .a = tri_1.a, .b = tri_1.b },
			{ .a = tri_1.b, .b = tri_1.c },
			{ .a = tri_1.c, .b = tri_1.a },
		};

		i32 counts[3] = {0,0,0};

		for (i32 idx_2 = 0; idx_2 < in_num_tris; ++idx_2)
		{
			if (idx_2 == idx_1) { continue; }

			const ConvexTri tri_2 = in_tris[idx_2];
			ConvexEdge edges_2[3] =
			{
				{ .a = tri_2.a, .b = tri_2.b },
				{ .a = tri_2.b, .b = tri_2.c },
				{ .a = tri_2.c, .b = tri_2.a },
			};	

			for (i32 edges_1_idx = 0; edges_1_idx < 3; ++edges_1_idx)
			{
				if (convex_edge_equals(edges_1[edges_1_idx], edges_2[0])) { counts[edges_1_idx] += 1; }
				if (convex_edge_equals(edges_1[edges_1_idx], edges_2[1])) { counts[edges_1_idx] += 1; }
				if (convex_edge_equals(edges_1[edges_1_idx], edges_2[2])) { counts[edges_1_idx] += 1; }
			}	
		}

		// An edge that isn't shared is dangling
		for (i32 edges_1_idx = 0; edges_1_idx < 3; ++edges_1_idx)
		{
			if (counts[edges_1_idx] == 0)
			{
				sb_push(*out_dangling_edges, edges_1[edges_1_idx]);
			}
		}		
	}
}

