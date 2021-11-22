#pragma once

#include "vec.h"
#include "quat.h"
#include "matrix.h"
#include "stretchy_buffer.h"
#include "mpr.h"

Vec3 cube_points[] = {
    {-1, -1, -1},
    {-1,  1, -1},
    {-1, -1,  1},
    {-1,  1,  1},
    { 1,  1,  1},
    { 1,  1, -1},
    { 1, -1,  1},
    { 1, -1, -1},   
};

typedef struct Collider {
    Vec3* convex_points;
    
    Vec3 position;
    Vec3 velocity;
    Vec3 pending_force;

    Quat rotation;
    Vec3 angular_velocity;
    Vec3 pending_torque;

    Vec3 scale;

    //TODO: float center_of_mass_offset;
    float mass;
    float moment_of_inertia;
    float friction;
    float restitution;

    bool is_kinematic;
} Collider;

Collider make_cube_collider() {
    Collider out_cube = {
        .convex_points = NULL,
        .position = vec3_new(0,0,0),
        .velocity = vec3_new(0,0,0),
        .pending_force = vec3_new(0,0,0),
        .rotation = quat_identity,
        .angular_velocity = vec3_new(0,0,0),
        .pending_torque = vec3_new(0,0,0),
        .scale = vec3_new(1,1,1),
        .mass = 5,
        .moment_of_inertia = 2.5,
        .friction = 0,
        .restitution = 0,
        .is_kinematic = true,
    };

    uint32_t num_points = sizeof(cube_points) / sizeof(Vec3);
    sb_add(out_cube.convex_points, num_points);
    memcpy(out_cube.convex_points, cube_points, sizeof(cube_points));

    return out_cube;
}

void physics_add_force(Collider* in_collider, Vec3 force) {
    in_collider->pending_force = vec3_add(in_collider->pending_force, force);
}

void physics_add_torque(Collider* in_collider, Vec3 torque) {
    in_collider->pending_torque = vec3_add(in_collider->pending_torque, torque);
}

void physics_add_torque_at_location(Collider* in_collider, Vec3 force , Vec3 location) {
    Vec3 lever_arm = vec3_sub(location, in_collider->position);
    Vec3 torque = vec3_cross(force, lever_arm);
    physics_add_torque(in_collider, torque);
}

void physics_add_force_at_location(Collider* in_collider, Vec3 force, Vec3 location) {
    physics_add_torque_at_location(in_collider, force, location);
    physics_add_force(in_collider, force);
}

void physics_resolve_collision(Collider* in_collider, const Vec3 hit_loc, const Vec3 hit_dir, const float restitution, const float delta_time, size_t num_iterations) {
    if (in_collider->is_kinematic) {
        const float position_correction_scalar = 0.05f; //TODO:
        in_collider->position = vec3_add(in_collider->position, vec3_scale(vec3_normalize(hit_dir), position_correction_scalar));
        
        { 
            //Stopping accel: accel required to stop object's current velocity F = m * dv / dt
            Vec3 impulse_a = vec3_scale(hit_dir, vec3_length(in_collider->velocity) * restitution);
            impulse_a = vec3_scale(impulse_a, 1.0f/(float)num_iterations);
            impulse_a = vec3_scale(impulse_a, in_collider->mass / delta_time);
            physics_add_force(in_collider, impulse_a);
        }

        {
            //TODO: should this scaled by velocity?
            Vec3 torque_a = vec3_scale(hit_dir, vec3_length(in_collider->velocity) * restitution);
            torque_a = vec3_scale(torque_a, 1.0f/(float)num_iterations);
            torque_a = vec3_scale(torque_a, 1.0f / delta_time);
            physics_add_torque_at_location(in_collider, torque_a, hit_loc);
        }
    }
}

void physics_run_simulation(Collider* in_colliders, float delta_time) {
    uint32_t collider_count = sb_count(in_colliders);

    //1. TODO: Apply new Forces / Torques to in_colliders (acceleration update)
    //2. Compute new velocity based on acceleration (velocity update)
    //3. Move objects based on velocity (position/rotation update)
    for (uint32_t i = 0; i < collider_count; ++i) {
        Collider* collider = &in_colliders[i];

        // Acceleration this frame: pending_force * delta_time / mass;
        Vec3 acceleration = vec3_scale(collider->pending_force, delta_time / collider->mass);
        collider->velocity = vec3_add(collider->velocity, acceleration);
        collider->pending_force = vec3_zero;

        Vec3 delta_position = vec3_scale(collider->velocity, delta_time);
        collider->position = vec3_add(collider->position, delta_position);

        //Torque
        Vec3 angular_acceleration = vec3_scale(collider->pending_torque, delta_time / collider->moment_of_inertia);
        collider->angular_velocity = vec3_add(collider->angular_velocity, angular_acceleration);
        collider->pending_torque = vec3_zero;

        float angular_velocity_magnitude = vec3_length(collider->angular_velocity) * delta_time;

        if (angular_velocity_magnitude != 0.0f)
        {
            Vec3 angular_velocity_axis = vec3_normalize(collider->angular_velocity);
            
            Quat delta_rotation = quat_new(angular_velocity_axis, angular_velocity_magnitude);
            collider->rotation = quat_mult(collider->rotation, delta_rotation);
            collider->rotation = quat_normalize(collider->rotation);
        }
    }

    static const size_t num_iterations = 4;
    for (size_t i = 0; i < num_iterations; ++i)
    {
        //4. Check for collisions
        for (uint32_t index_a = 0; index_a < collider_count; ++index_a) {
            for (uint32_t index_b = index_a + 1; index_b < collider_count; ++index_b) {
                Collider* collider_a = &in_colliders[index_a];
                Collider* collider_b = &in_colliders[index_b];

                Mat4 scale_a = mat4_scale(collider_a->scale);
                Mat4 rotation_a = quat_to_mat4(collider_a->rotation);
                Mat4 translation_a = mat4_translation(collider_a->position);
                Mat4 transform_a = mat4_mult_mat4(mat4_mult_mat4(scale_a, rotation_a), translation_a);

                Mat4 scale_b = mat4_scale(collider_b->scale);
                Mat4 rotation_b = quat_to_mat4(collider_b->rotation);
                Mat4 translation_b = mat4_translation(collider_b->position);
                Mat4 transform_b = mat4_mult_mat4(mat4_mult_mat4(scale_b, rotation_b), translation_b);

                //hit_normal is scaled by penetration depth?
                MprVec3 hit_normal, contact_a, contact_b;
                int result = mpr_check_collision((MprVec3*) collider_a->convex_points, sb_count(collider_a->convex_points), (float*) transform_a.d,
                                                (MprVec3*) collider_b->convex_points, sb_count(collider_b->convex_points), (float*) transform_b.d,
                                                &hit_normal, &contact_a, &contact_b);

                if (result) {

                    Vec3 hit_dir_a = vec3_new(hit_normal.x, hit_normal.y, hit_normal.z);
                    Vec3 hit_dir_b = vec3_negate(hit_dir_a);

                    //TODO: Add to ea. collider, and combine in some way both collider's restitution
                    static const float restitution = 0.75f;

                    physics_resolve_collision(collider_a, vec3_new(contact_a.x, contact_a.y, contact_a.z), hit_dir_a, restitution, delta_time, num_iterations);
                    physics_resolve_collision(collider_b, vec3_new(contact_b.x, contact_b.y, contact_b.z), hit_dir_b, restitution, delta_time, num_iterations);
                }
            }
        }
    }

    //FCS TODO: Handle collision response outside of loop above, and sort by severity?
}