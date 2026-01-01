#pragma once

#include "basic_types.h"
#include "math/math_lib.h"
#include "stretchy_buffer.h"

typedef struct Bounds
{
	Vec3 min;
	Vec3 max;
} Bounds;

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

typedef enum ShapeType
{
	SHAPE_TYPE_SPHERE,
} ShapeType;

typedef struct SphereShape
{
	float radius;
} SphereShape;

typedef struct Shape
{
	ShapeType type;	
	union
	{
		SphereShape sphere;
	};

	Vec3 center_of_mass;
} Shape;

typedef struct PhysicsBody
{
	Vec3 position;
	Quat orientation;
	Vec3 linear_velocity;
	Vec3 angular_velocity;
	Shape shape;
	float inverse_mass;
	float elasticity;
	float friction;
} PhysicsBody;

typedef struct PhysicsContact
{
	Vec3 point_on_a_world;	
	Vec3 point_on_b_world;
	Vec3 point_on_a_local;
	Vec3 point_on_b_local;

	Vec3 normal;
	float separation_distance;
	float time_of_impact;

	PhysicsBody* body_a;
	PhysicsBody* body_b;
} PhysicsContact;

Bounds physics_body_get_bounds(const PhysicsBody* in_body)
{
	Bounds out_bounds = {
		.min = vec3_new(FLT_MAX, FLT_MAX, FLT_MAX),
		.max = vec3_new(-FLT_MAX, -FLT_MAX, -FLT_MAX),
	};

	switch(in_body->shape.type)
	{
		case SHAPE_TYPE_SPHERE:
		{
			const float radius = in_body->shape.sphere.radius;
			out_bounds = (Bounds) {
				.min = vec3_sub(in_body->position, vec3_new(radius, radius, radius)),
				.max = vec3_add(in_body->position, vec3_new(radius, radius, radius)),
			};
			break;
		}
		default:
		{
			assert(false);
		}
	}

	return out_bounds;
}

Vec3 physics_body_local_to_world_space(PhysicsBody* in_body, Vec3 in_body_space_point)
{
	Vec3 rotated = quat_rotate_vec3(in_body->orientation, in_body_space_point);
	Vec3 translated = vec3_add(rotated, in_body->position);
	return translated;
}

Vec3 physics_body_world_to_local_space(PhysicsBody* in_body, Vec3 in_world_point)
{
	Vec3 untranslated = vec3_sub(in_world_point, in_body->position);
	Vec3 unrotated = quat_rotate_vec3(quat_inverse(in_body->orientation), untranslated);
	return unrotated;
}

Vec3 physics_body_get_center_of_mass_world(PhysicsBody* in_body)
{
	Vec3 model_space = in_body->shape.center_of_mass;
	return physics_body_local_to_world_space(in_body, model_space);
}

Mat3 physics_body_get_inertia_tensor_matrix(PhysicsBody* in_body)
{
	switch (in_body->shape.type)
	{
		case SHAPE_TYPE_SPHERE:			
		{
			const float sphere_radius = in_body->shape.sphere.radius;
			const float sphere_tensor_value = (2.0f / 5.0f) * sphere_radius * sphere_radius;
			Mat3 sphere_tensor = {
				.d[0][0] = sphere_tensor_value,
				.d[1][1] = sphere_tensor_value,
				.d[2][2] = sphere_tensor_value,
			};
			return sphere_tensor;
		}
	}
}

Mat3 physics_body_get_inverse_inertia_tensor_local(PhysicsBody* in_body)
{
	switch (in_body->shape.type)
	{
		case SHAPE_TYPE_SPHERE:			
		{
			//FCS TODO: This code might be common to all shapes...
			// compute inverse of inertia tensor matrix
			Mat3 result = optional_get(mat3_inverse(physics_body_get_inertia_tensor_matrix(in_body)));
			// scale that matrix by inverse mass
			result = mat3_mul_f32(result, in_body->inverse_mass);
			// return that matrix
			return result;
		}
	}
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
	const float max_angular_speed = 30.0f;
	const float max_angular_speed_squared = max_angular_speed * max_angular_speed;
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

void physics_body_update(PhysicsBody* in_body, float in_delta_time)
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
				physics_body_get_inertia_tensor_matrix(in_body)
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
	const float in_sphere_radius, 
	float* out_t1, 
	float* out_t2
)
{
	const Vec3 m = vec3_sub(in_sphere_center, in_ray_start);
	const float a = vec3_dot(in_ray_dir, in_ray_dir);
	const float b = vec3_dot(m, in_ray_dir);
	const float c = vec3_dot(m,m) - (in_sphere_radius * in_sphere_radius);
	const float delta = b * b - a * c;
	if (delta < 0)
	{
		// no real solutions exist
		return false;
	}
	
	const float inv_a = 1.0f / a;
	const float delta_root = sqrtf(delta);
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
	const float in_delta_time,
	Vec3* out_point_on_a,
	Vec3* out_point_on_b,
	float* out_time_of_impact
)
{
	const Vec3 relative_velocity = vec3_sub(in_vel_a, in_vel_b);
	const Vec3 ray_start = in_pos_a;
	const Vec3 ray_end = vec3_add(ray_start, vec3_scale(relative_velocity, in_delta_time));
	const Vec3 ray_dir = vec3_sub(ray_end, ray_start);

	const Vec3 combined_sphere_pos = in_pos_b;
	const float combined_sphere_radius = in_sphere_a->radius + in_sphere_b->radius;

	float t0 = 0;
	float t1 = 0;

	if (vec3_length_squared(ray_dir) < 0.001f * 0.001f)
	{
		// ray_dir is too short, just check if already intersecting
		const Vec3 a_to_b = vec3_sub(in_pos_b, in_pos_a);
		const float radius = combined_sphere_radius + 0.001f;
		const float radius_squared = radius * radius;
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

	const float time_of_impact = (t0 < 0.0f) ? 0.0f : t0;

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

bool physics_body_intersect(PhysicsBody* in_body_a, PhysicsBody* in_body_b, const float in_delta_time, PhysicsContact* in_contact)
{
	in_contact->body_a = in_body_a;
	in_contact->body_b = in_body_b;

	const Vec3 pos_a = in_body_a->position;
	const Vec3 pos_b = in_body_b->position;

	const Vec3 vel_a = in_body_a->linear_velocity;	
	const Vec3 vel_b = in_body_b->linear_velocity;

	assert(in_body_a->shape.type == SHAPE_TYPE_SPHERE);
	assert(in_body_b->shape.type == SHAPE_TYPE_SPHERE);

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
	const float elasticity = body_a->elasticity * body_b->elasticity;

	const float inverse_mass_a = body_a->inverse_mass;
	const float inverse_mass_b = body_b->inverse_mass;
	const float inverse_mass_sum = inverse_mass_a + inverse_mass_b;
	
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
		const float angular_factor = vec3_dot(vec3_add(angular_ja, angular_jb), contact_normal);

		// Calculate collision impulse magnitude
		const float collision_impulse_magnitude = (1.0f + elasticity) * vec3_dot(velocity_a_to_b, contact_normal) / (inverse_mass_sum + angular_factor);
		// Collision impulse is in direction of contact normal
		const Vec3 collision_impulse = vec3_scale(contact_normal, collision_impulse_magnitude);

		// Apply our collision impulses to both bodies
		physics_body_apply_impulse(body_a, vec3_negate(collision_impulse), point_on_a);
		physics_body_apply_impulse(body_b, collision_impulse, point_on_b);
	}

	{	// Friction Impulse

		// Calculate friction values
		const float friction_a = body_a->friction;
		const float friction_b = body_b->friction;
		// Total friction is product of both bodies' friction
		const float friction = friction_a * friction_b;

		// Scale contact normal based on similarity of contact normal to velocity_a_to_b
		const Vec3 velocity_normal = vec3_scale(contact_normal, vec3_dot(contact_normal, velocity_a_to_b));
		const Vec3 velocity_tangent = vec3_sub(velocity_a_to_b, velocity_normal);
		const Vec3 relative_velocity_tangent = vec3_normalize(velocity_tangent);

		// Compute inertia values based on inverse inertia tensors, direction to points, and relative velocity tangent
		const Vec3 inertia_a = vec3_cross(mat3_mul_vec3(inverse_inertia_tensor_a, vec3_cross(ra, relative_velocity_tangent)), ra);
		const Vec3 inertia_b = vec3_cross(mat3_mul_vec3(inverse_inertia_tensor_b, vec3_cross(rb, relative_velocity_tangent)), rb);
		// Compute final inverse inertia
		const float inverse_inertia = vec3_dot(vec3_add(inertia_a, inertia_b), relative_velocity_tangent);

		// Reduce mass by inverse_inertia
		const float reduced_mass = 1.0f / (inverse_mass_sum + inverse_inertia);
		// Use reduced mass to compute friction impulse
		const Vec3 friction_impulse = vec3_scale(velocity_tangent, reduced_mass * friction);
		physics_body_apply_impulse(body_a, vec3_negate(friction_impulse), point_on_a);
		physics_body_apply_impulse(body_b, friction_impulse, point_on_b);
	}

	// Resolve interpenetration 
	if (in_contact->time_of_impact == 0.0f)
	{

		// Move colliding objects to just outside of each other
		const float ta = body_a->inverse_mass / inverse_mass_sum;
		const float tb = body_b->inverse_mass / inverse_mass_sum;

		const Vec3 ds = vec3_sub(in_contact->point_on_b_world, in_contact->point_on_a_world);
		body_a->position = vec3_add(body_a->position, vec3_scale(ds, ta));
		body_b->position = vec3_add(body_b->position, vec3_scale(ds,-tb));
	}
}

i32 physics_contact_compare(const void* in_a, const void* in_b)
{
	const PhysicsContact* in_contact_a = (const PhysicsContact*) in_a;
	const PhysicsContact* in_contact_b = (const PhysicsContact*) in_b;

	const float toi_a = in_contact_a->time_of_impact;
	const float toi_b = in_contact_b->time_of_impact;
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
	int id;
	float value;
	bool is_min;
} PseudoPhysicsBody;

int pseudo_physics_body_compare(const void* a, const void* b)
{
	const PseudoPhysicsBody* pseudo_body_a = (const PseudoPhysicsBody*) a;
	const PseudoPhysicsBody* pseudo_body_b = (const PseudoPhysicsBody*) b;
	return (pseudo_body_a->value < pseudo_body_b->value) ? -1 : 1;
}

sbuffer(PseudoPhysicsBody) pseudo_physics_bodies_create_sorted(sbuffer(PhysicsBody) in_bodies, const float in_delta_time)
{
	const i32 num_bodies = sb_count(in_bodies);
	const i32 num_pseudo_bodies = num_bodies * 2;

	sbuffer(PseudoPhysicsBody) out_pseudo_bodies = NULL;
	sb_reserve(out_pseudo_bodies, num_pseudo_bodies);

	const Vec3 axis = vec3_normalize(vec3_new(1,1,1));

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
		const float epsilon = 0.01f;
		bounds_expand_point(&bounds, vec3_add(bounds.min, vec3_scale(vec3_new(-1,-1,-1), epsilon)));
		bounds_expand_point(&bounds, vec3_add(bounds.max, vec3_scale(vec3_new( 1, 1, 1), epsilon)));

		// Create a min and max PseudoPhysicsBody by projecting the bounds min and max onto our 1D axis
		PseudoPhysicsBody new_pseudo_body_min = {
			.id = body_idx,
			.value = vec3_dot(axis, bounds.min),
			.is_min = true,
		};
		sb_push(out_pseudo_bodies, new_pseudo_body_min);

		PseudoPhysicsBody new_pseudo_body_max = {
			.id = body_idx,
			.value = vec3_dot(axis, bounds.max),
			.is_min = false,
		};
		sb_push(out_pseudo_bodies, new_pseudo_body_max);
	}

	// Sort pseudo bodies by their value, which is dot(axis, bounds.min) or dot(axis, bounnds.max) for a body
	qsort(out_pseudo_bodies, num_pseudo_bodies, sizeof(PseudoPhysicsBody), pseudo_physics_body_compare);

	return out_pseudo_bodies;
}

//FCS TODO: Rename to idx_a, idx_b
typedef struct CollisionPair
{
	int id_a;
	int id_b;
} CollisionPair;

bool collision_pair_equals(CollisionPair* in_lhs, CollisionPair* in_rhs)
{
	return	(		(in_lhs->id_a == in_rhs->id_a)
				&&	(in_lhs->id_b == in_rhs->id_b))
		||	(		(in_lhs->id_a == in_rhs->id_b)
				&&	(in_lhs->id_b == in_rhs->id_a));
}

sbuffer(CollisionPair) physics_scene_broad_phase(PhysicsScene* in_physics_scene, float in_delta_time)
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
				.id_a = pseudo_body_a->id,
			};

			for (i32 j = i+1; j < num_pseudo_bodies; ++j)		
			{
				PseudoPhysicsBody* pseudo_body_b = &sorted_pseudo_bodies[j];

				// if we hit pseudo_body_a's own max, we stop looking
				if(pseudo_body_b->id == pseudo_body_a->id) { break;}

				// We only record a collision pair if we hit the MIN point of Body B	
				if (!pseudo_body_b->is_min) { continue; }
				
				new_collision_pair.id_b = pseudo_body_b->id;
				sb_push(out_collision_pairs, new_collision_pair);
			}
		}

		// Free pseudo bodies
		sb_free(sorted_pseudo_bodies);
	}

	// return collision pairs
	return out_collision_pairs;
}

void physics_scene_update(PhysicsScene* in_physics_scene, float in_delta_time)
{
	const i32 num_bodies = sb_count(in_physics_scene->bodies);

	// Acceleration due to gravity
	for (i32 body_idx = 0; body_idx < num_bodies; ++body_idx)
	{
		PhysicsBody* body = &in_physics_scene->bodies[body_idx];

		if (body->inverse_mass > 0.f)
		{
			float mass = 1.0f / body->inverse_mass;
			Vec3 impulse_gravity = vec3_scale(vec3_new(0.f, -10.f, 0.f), mass * in_delta_time);
			physics_body_apply_impulse_linear(body, impulse_gravity);
		}
	}

	// Broadphase
	sbuffer(CollisionPair) collision_pairs = physics_scene_broad_phase(in_physics_scene, in_delta_time);

	printf("------------------------------\n");
	printf("Num Collision Pairs: %i\n", sb_count(collision_pairs));
	printf("Max Possible Pairs:  %i\n", (num_bodies * (num_bodies-1)) / 2);
	printf("------------------------------\n");

	// Narrowphase
	const i32 max_contacts = num_bodies * num_bodies;
	sbuffer(PhysicsContact) contacts = NULL;
	sb_reserve(contacts, max_contacts);

	for (i32 pair_idx = 0; pair_idx < sb_count(collision_pairs); ++pair_idx)
	{
		CollisionPair* collision_pair = &collision_pairs[pair_idx];
		PhysicsBody* body_a = &in_physics_scene->bodies[collision_pair->id_a];
		PhysicsBody* body_b = &in_physics_scene->bodies[collision_pair->id_b];

		// Skip if both bodies have zero mass
		if (body_a->inverse_mass <= 0.f && body_b->inverse_mass <= 0.f)
		{
			continue;
		}

		PhysicsContact contact = {};
		if (physics_body_intersect(body_a, body_b, in_delta_time, &contact))
		{
			sb_push(contacts, contact);
		}
	}

	// Free collision pairs
	sb_free(collision_pairs);

	// Sort contacts by time of impact
	const i32 num_contacts = sb_count(contacts);
	if (num_contacts > 1)
	{
		qsort(contacts, num_contacts, sizeof(PhysicsContact), physics_contact_compare);
	}

	// peform physics body updates and contact resolution at each contact time of impact
	float accumulated_delta_time = 0.f;
	for (i32 contact_idx = 0; contact_idx < sb_count(contacts); ++contact_idx)
	{
		PhysicsContact* contact = &contacts[contact_idx];

		const float contact_delta_time = contact->time_of_impact - accumulated_delta_time;

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
	const float remaining_delta_time = in_delta_time - accumulated_delta_time;
	if (remaining_delta_time > 0.0f)
	{
		for (i32 body_idx = 0; body_idx < num_bodies; ++body_idx)
		{
			PhysicsBody* body = &in_physics_scene->bodies[body_idx];
			physics_body_update(body, remaining_delta_time);
		}
	}
}

