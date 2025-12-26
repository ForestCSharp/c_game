#pragma once

#include "basic_types.h"
#include "math/math_lib.h"
#include "stretchy_buffer.h"

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
	in_body->linear_velocity = vec3_add(in_body->linear_velocity, vec3_scale(in_impulse, in_body->inverse_mass));
}

void physics_body_apply_impulse_angular(PhysicsBody* in_body, Vec3 in_impulse)
{	
	if (in_body->inverse_mass <= 0.f) { return; }

	const Vec3 delta_angular_velocity = mat3_mul_vec3(physics_body_get_inverse_inertia_tensor_world(in_body), in_impulse);
	in_body->angular_velocity = vec3_add(in_body->angular_velocity, delta_angular_velocity);

	const float max_angular_speed = 30.0f;
	const float max_angular_speed_squared = max_angular_speed * max_angular_speed;
	if (vec3_length_squared(in_body->angular_velocity) > max_angular_speed_squared)
	{
		in_body->angular_velocity = vec3_scale(vec3_normalize(in_body->angular_velocity), max_angular_speed);
	}
}

void physics_body_apply_impulse(PhysicsBody* in_body, Vec3 in_impulse, Vec3 in_location)
{
	physics_body_apply_impulse_linear(in_body, in_impulse);

	Vec3 center_of_mass = physics_body_get_center_of_mass_world(in_body);	
	Vec3 r = vec3_sub(in_location, center_of_mass);
	Vec3 impulse_angular = vec3_cross(r, in_impulse);
	physics_body_apply_impulse_angular(in_body, impulse_angular);
}

void physics_body_update(PhysicsBody* in_body, float in_delta_time)
{	
	// Update position based on linear velocity
	in_body->position = vec3_add(in_body->position, vec3_scale(in_body->linear_velocity, in_delta_time));

	// Update orientation and position based on angular velocity
	Vec3 center_of_mass = physics_body_get_center_of_mass_world(in_body);
	Vec3 center_of_mass_to_position = vec3_sub(in_body->position, center_of_mass);
	
	Mat3 orientation = quat_to_mat3(in_body->orientation);
	Mat3 inertia_tensor = 
		mat3_mul_mat3(
			mat3_mul_mat3(
				orientation, 
				physics_body_get_inertia_tensor_matrix(in_body)
			),
			mat3_transpose(orientation)	
		);
	optional(Mat3) inverse_inertia_tensor = mat3_inverse(inertia_tensor);
	assert(optional_is_set(inverse_inertia_tensor));

	Vec3 alpha = mat3_mul_vec3(
					optional_get(inverse_inertia_tensor),
					vec3_cross(
						in_body->angular_velocity, 
						mat3_mul_vec3(inertia_tensor, in_body->angular_velocity)
					)
				);
	in_body->angular_velocity = vec3_add(in_body->angular_velocity, vec3_scale(alpha, in_delta_time));

	Vec3 d_angle = vec3_scale(in_body->angular_velocity, in_delta_time);
	Quat d_quat = quat_new(d_angle, vec3_length(d_angle));
	in_body->orientation = quat_normalize(quat_mul(d_quat, in_body->orientation));
	in_body->position = vec3_add(center_of_mass, quat_rotate_vec3(d_quat, center_of_mass_to_position));
}

bool physics_body_intersect(PhysicsBody* in_body_a, PhysicsBody* in_body_b, PhysicsContact* in_contact)
{
	in_contact->body_a = in_body_a;
	in_contact->body_b = in_body_b;

	const Vec3 a_to_b = vec3_sub(in_body_b->position, in_body_a->position);
	in_contact->normal = vec3_normalize(a_to_b);

	assert(in_body_a->shape.type == SHAPE_TYPE_SPHERE);
	assert(in_body_b->shape.type == SHAPE_TYPE_SPHERE);

	const SphereShape* sphere_a = &in_body_a->shape.sphere;
	const SphereShape* sphere_b = &in_body_b->shape.sphere;

	in_contact->point_on_a_world = vec3_add(in_body_a->position, vec3_scale(in_contact->normal, sphere_a->radius));
	in_contact->point_on_b_world = vec3_sub(in_body_b->position, vec3_scale(in_contact->normal, sphere_b->radius));

	const float combined_radius = sphere_a->radius + sphere_b->radius;
	const float a_to_b_length_squared = vec3_length_squared(a_to_b);
	if (a_to_b_length_squared <= (combined_radius * combined_radius))
	{
		return true;
	}

	return false;
}

void physics_contact_resolve(PhysicsContact* in_contact)
{
	PhysicsBody* body_a = in_contact->body_a;
	PhysicsBody* body_b = in_contact->body_b;
	assert(body_a);
	assert(body_b);

	const Vec3 point_on_a = in_contact->point_on_a_world;
	const Vec3 point_on_b = in_contact->point_on_b_world;
	const float elasticity = body_a->elasticity * body_b->elasticity;

	const float inverse_mass_a = body_a->inverse_mass;
	const float inverse_mass_b = body_b->inverse_mass;
	const float inverse_mass_sum = inverse_mass_a + inverse_mass_b;
	
	const Mat3 inverse_inertia_a = physics_body_get_inverse_inertia_tensor_world(body_a);
	const Mat3 inverse_inertia_b = physics_body_get_inverse_inertia_tensor_world(body_b);

	const Vec3 contact_normal = in_contact->normal;

	const Vec3 ra = vec3_sub(point_on_a, physics_body_get_center_of_mass_world(body_a));
	const Vec3 rb = vec3_sub(point_on_b, physics_body_get_center_of_mass_world(body_b));

	const Vec3 angular_ja = vec3_cross(mat3_mul_vec3(inverse_inertia_a, vec3_cross(ra, contact_normal)), ra);
	const Vec3 angular_jb = vec3_cross(mat3_mul_vec3(inverse_inertia_b, vec3_cross(rb, contact_normal)), rb);
	const float angular_factor = vec3_dot(vec3_add(angular_ja, angular_jb), contact_normal);

	// Get world space velocity of motion and rotation
	const Vec3 velocity_a = vec3_add(body_a->linear_velocity, vec3_cross(body_a->angular_velocity, ra));	
	const Vec3 velocity_b = vec3_add(body_b->linear_velocity, vec3_cross(body_b->angular_velocity, rb));

	// Calculate collision impulse
	const Vec3 velocity_ab = vec3_sub(velocity_a, velocity_b);
	const float impulse_j = (1.0f + elasticity) * vec3_dot(velocity_ab, contact_normal) / (inverse_mass_sum + angular_factor);
	const Vec3 impulse_j_vector = vec3_scale(contact_normal, impulse_j);

	physics_body_apply_impulse(body_a, vec3_negate(impulse_j_vector), point_on_a);
	physics_body_apply_impulse(body_b, impulse_j_vector, point_on_b);

	// Calculate friction impulse
	const float friction_a = body_a->friction;
	const float friction_b = body_b->friction;
	const float friction = friction_a * friction_b;

	const Vec3 velocity_normal = vec3_scale(contact_normal, vec3_dot(contact_normal, velocity_ab));
	const Vec3 velocity_tangent = vec3_sub(velocity_ab, velocity_normal);
	const Vec3 relative_velocity_tangent = vec3_normalize(velocity_tangent);

	const Vec3 inertia_a = vec3_cross(mat3_mul_vec3(inverse_inertia_a, vec3_cross(ra, relative_velocity_tangent)), ra);
	const Vec3 inertia_b = vec3_cross(mat3_mul_vec3(inverse_inertia_b, vec3_cross(rb, relative_velocity_tangent)), rb);
	const float inverse_inertia = vec3_dot(vec3_add(inertia_a, inertia_b), relative_velocity_tangent);

	const float reduced_mass = 1.0f / (inverse_mass_sum + inverse_inertia);
	const Vec3 impulse_friction = vec3_scale(velocity_tangent, reduced_mass * friction);
	physics_body_apply_impulse(body_a, vec3_negate(impulse_friction), point_on_a);
	physics_body_apply_impulse(body_b, impulse_friction, point_on_b);

	// Move colliding objects to just outside of each other
	const float ta = body_a->inverse_mass / inverse_mass_sum;
	const float tb = body_b->inverse_mass / inverse_mass_sum;

	const Vec3 ds = vec3_sub(in_contact->point_on_b_world, in_contact->point_on_a_world);
	body_a->position = vec3_add(body_a->position, vec3_scale(ds, ta));
	body_b->position = vec3_add(body_b->position, vec3_scale(ds,-tb));
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

void physics_scene_update(PhysicsScene* in_physics_scene, float in_delta_time)
{
	// Acceleration due to gravity
	for (i32 body_idx = 0; body_idx < sb_count(in_physics_scene->bodies); ++body_idx)
	{
		PhysicsBody* body = &in_physics_scene->bodies[body_idx];

		if (body->inverse_mass > 0.f)
		{
			float mass = 1.0f / body->inverse_mass;
			Vec3 impulse_gravity = vec3_scale(vec3_new(0.f, -10.f, 0.f), mass * in_delta_time);
			physics_body_apply_impulse_linear(body, impulse_gravity);
		}

	}

	// Collision Test
	for (i32 body_a_idx = 0; body_a_idx < sb_count(in_physics_scene->bodies); ++body_a_idx)
	{
		for (i32 body_b_idx = body_a_idx + 1; body_b_idx < sb_count(in_physics_scene->bodies); ++body_b_idx)
		{
			PhysicsBody* body_a = &in_physics_scene->bodies[body_a_idx];
			PhysicsBody* body_b = &in_physics_scene->bodies[body_b_idx];

			// Skip if both bodies have zero mass
			if (body_a->inverse_mass <= 0.f && body_b->inverse_mass <= 0.f)
			{
				continue;
			}

			PhysicsContact contact = {};
			if (physics_body_intersect(body_a, body_b, &contact))
			{
				physics_contact_resolve(&contact);
			}
		}
	}

	// Position Update
	for (i32 body_idx = 0; body_idx < sb_count(in_physics_scene->bodies); ++body_idx)
	{
		PhysicsBody* body = &in_physics_scene->bodies[body_idx];
		physics_body_update(body, in_delta_time);
	}
}

