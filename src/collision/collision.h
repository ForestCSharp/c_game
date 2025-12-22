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
	Shape shape;
	float inverse_mass;
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

void physics_body_apply_impulse_linear(PhysicsBody* in_body, Vec3 in_impulse)
{
	if (in_body->inverse_mass <= 0.f) { return; }
	in_body->linear_velocity = vec3_add(in_body->linear_velocity, vec3_scale(in_impulse, in_body->inverse_mass));
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

//FCS TODO: Move up
void physics_contact_resolve(PhysicsContact* in_contact)
{
	PhysicsBody* body_a = in_contact->body_a;
	PhysicsBody* body_b = in_contact->body_b;

	assert(body_a);
	assert(body_b);

	const float inverse_mass_sum = body_a->inverse_mass + body_b->inverse_mass;

	// Calculate collision impulse
	const Vec3 velocity_a_b = vec3_sub(body_a->linear_velocity, body_b->linear_velocity);
	const float impulse_j = -2.0f * vec3_dot(velocity_a_b, in_contact->normal) / inverse_mass_sum;
	const Vec3 impulse_vector = vec3_scale(in_contact->normal, impulse_j);
	physics_body_apply_impulse_linear(body_a, impulse_vector);
	physics_body_apply_impulse_linear(body_b, vec3_negate(impulse_vector));

	// Move colliding objects to just outside of each other
	const float ta = body_a->inverse_mass / inverse_mass_sum;
	const float tb = body_b->inverse_mass / inverse_mass_sum;

	const Vec3 ds = vec3_sub(in_contact->point_on_b_world, in_contact->point_on_a_world);
	body_a->position = vec3_add(body_a->position, vec3_scale(ds, ta));
	body_b->position = vec3_add(body_b->position, vec3_scale(ds,-tb));
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
		body->position = vec3_add(body->position, vec3_scale(body->linear_velocity, in_delta_time));
	}
}

