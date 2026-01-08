#pragma once

#include "basic_types.h"
#include "math/math_lib.h"

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

Vec3 bounds_get_dimensions(const Bounds* in_bounds)
{
	return vec3_sub(in_bounds->max, in_bounds->min);
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

i32 furthest_point_in_dir(const Vec3* in_points, const i32 in_num_points, const Vec3 in_dir)
{
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

typedef struct ConvexHullTri 
{
	i32 a;
	i32 b;
	i32 c;
} ConvexHullTri;

typedef struct ConvexHull
{
	sbuffer(Vec3) points;
	sbuffer(ConvexHullTri) tris;
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
	sb_push(out_hull.tris, ((ConvexHullTri) {
		.a = 0,
		.b = 1,
		.c = 2,
	}));
	sb_push(out_hull.tris, ((ConvexHullTri) {
		.a = 0,
		.b = 2,
		.c = 3,
	}));
	sb_push(out_hull.tris, ((ConvexHullTri) {
		.a = 2,
		.b = 1,
		.c = 3,
	}));
	sb_push(out_hull.tris, ((ConvexHullTri) {
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
		const Vec3 point = *in_points[i];
		bool is_external = false;
		for (int t = 0; t < sb_count(in_convex_hull->tris); ++t)
		{
			const ConvexHullTri* tri = &in_convex_hull->tris[t];
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
		const Vec3 point = *in_points[i];
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

bool is_edge_unique(sbuffer(ConvexHullTri) in_tris, sbuffer(i32) in_facing_tri_idices, i32 in_ignore_tri, const ConvexEdge in_edge)
{
	for (i32 i = 0; i < sb_count(in_facing_tri_idices); ++i)
	{
		const i32 tri_idx = in_facing_tri_idices[i];
		if (tri_idx == in_ignore_tri)
		{
			continue;
		}

		const ConvexHullTri tri = in_tris[tri_idx];
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

//FCS TODO: HERE: Check functions below this line

void convex_hull_add_point(ConvexHull* in_convex_hull, const Vec3 in_point)
{
	// Find all triangles that face this point
	sbuffer(i32) facing_tri_indices = NULL;
	for (i32 i = sb_count(in_convex_hull->tris); i >= 0; --i)
	{
		const ConvexHullTri tri = in_convex_hull->tris[i];
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
		const ConvexHullTri tri = in_convex_hull->tris[tri_idx];

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
		sb_push(in_convex_hull->tris, ((ConvexHullTri) {
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
			ConvexHullTri tri = in_convex_hull->tris[tri_idx];
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
			ConvexHullTri* tri = &in_convex_hull->tris[tri_idx];
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
		const ConvexHullTri tri = in_convex_hull->tris[t];
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

Vec3 convex_hull_calculate_center_of_mass(const ConvexHull* in_convex_hull)
{
	const i32 num_samples = 100;

	Bounds bounds = bounds_init();
	bounds_expand_points(&bounds, in_convex_hull->points, sb_count(in_convex_hull->points));

	Vec3 bounds_dimensions = bounds_get_dimensions(&bounds);

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

	Vec3 bounds_dimensions = bounds_get_dimensions(&bounds);

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

Vec2 signed_volume_1D(const Vec3 s1, const Vec3 s2)
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

	if ((a <= b && p <= a) || (a >= b && b >= a))
	{
		return vec2_new(1.0f, 0.0f);
	}

	return vec2_new(0.0f, 1.0f);
}

Vec3 signed_volume_2D(const Vec3 s1, const Vec3 s2, const Vec3 s3)
{
	const Vec3 normal = vec3_cross(vec3_sub(s2, s1), vec3_sub(s3, s1));
	const Vec3 p0 = vec3_scale(normal, vec3_dot(s1, normal) / vec3_length_squared(normal));

	// Find axis with greatest projected area
	i32 idx = 0;
	f32 area_max = 0.f;
	f32 area_max_squared = 0.f;
	for (i32 i = 0; i < 3; ++i)
	{
		const i32 j = (i+1) % 3;		
		const i32 k = (i+2) % 3;

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
		const i32 j = (i+1) % 3;		
		const i32 k = (i+2) % 3;
		
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
		const i32 k = (i+1) % 3;		
		const i32 l = (i+2) % 3;

		const Vec3 edges_pts[3] = {
			s1,
			s2,
			s3
		};

		Vec2 lambda_edge = signed_volume_1D(edges_pts[k], edges_pts[l]);
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


Vec4 signed_volume_3D(const Vec3 s1, const Vec3 s2, const Vec3 s3, const Vec3 s4)
{
	Mat4 m = {
		.columns = {
			vec4_new(s1.x, s1.y, s1.z, 1.0f),
			vec4_new(s2.x, s2.y, s2.z, 1.0f),
			vec4_new(s3.x, s3.y, s3.z, 1.0f),
			vec4_new(s4.x, s4.y, s4.z, 1.0f),
		}
	};

	Vec4 c4 = vec4_new(
		mat4_cofactor(m, 3, 0),
		mat4_cofactor(m, 3, 1),
		mat4_cofactor(m, 3, 2),
		mat4_cofactor(m, 3, 2)
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

		const Vec3 lambdas_face = signed_volume_2D(face_pts[i], face_pts[j], face_pts[k]);
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

#error: TODO: test_signed_volume_projection()
