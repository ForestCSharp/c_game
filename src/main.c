
#include <stdio.h>
#include <stdlib.h>

#include "memory/allocator.h"
#include "app/app.h"
#include "threading/threading.h"
#include "timer.h"

// GPU_IMPLEMENTATION_<BACKEND> set up in build.sh	
#include "gpu/gpu.c"

#include "math/math_lib.h"

#include "stretchy_buffer.h"

#include "model/static_model.h"
#include "model/animated_model.h"
#include "game_object/game_object.h"
#include "uniforms.h"
#include "gui.h"
#include "debug_draw.h"
#include "task/task.h"
#include "string_type.h"

#include "truetype.h"

typedef struct AnimationUpdateTaskData
{
	AnimatedModel* animated_model;
	f32 delta_time;
	f32 global_animation_rate;
	sbuffer(AnimatedModelComponent*) components_to_update;
} AnimationUpdateTaskData;

void animation_update_task(void* in_arg)
{
	AnimationUpdateTaskData* task_data = (AnimationUpdateTaskData*) in_arg;
	AnimatedModel* animated_model = task_data->animated_model;
	const f32 delta_time = task_data->delta_time;
	const f32 global_animation_rate = task_data->global_animation_rate;

	for (i32 component_idx = 0; component_idx < sb_count(task_data->components_to_update); ++component_idx)
	{
		AnimatedModelComponent* animated_model_component = task_data->components_to_update[component_idx];	
		if (animated_model_component)
		{
			animated_model_component->current_anim_time += (delta_time * animated_model_component->animation_rate * global_animation_rate);
			if (animated_model_component->current_anim_time > animated_model->baked_animation.end_time)
			{ 
				animated_model_component->current_anim_time = animated_model->baked_animation.start_time;
			}
			if (animated_model_component->current_anim_time < animated_model->baked_animation.start_time)
			{ 
				animated_model_component->current_anim_time = animated_model->baked_animation.end_time;
			}
			animated_model_update_animation(
				animated_model, 
				animated_model_component->current_anim_time,
				animated_model_component->mapped_buffer_data
			);
		}
	}

	//FCS TODO: Tasks should have an arena that gets automatically freed so we don't have to manually manage this
	sb_free(task_data->components_to_update);
	FCS_MEM_FREE(task_data);
}

typedef struct Character
{
	GameObjectHandle root_object_handle;
	GameObjectHandle body_object_handle;
	GameObjectHandle head_object_handle;
	GameObjectHandle legs_object_handle;	
	GameObjectHandle left_arm_object_handle;
	GameObjectHandle right_arm_object_handle;
	GameObjectHandle camera_root_object_handle;
	GameObjectHandle camera_object_handle;

} Character;

// The mech's root is at its torso. Center the upright collider between its head and feet.
static Vec3 character_collider_center(TRS root)
{
	return vec3_add(root.translation, vec3_new(0, -4.5f * root.scale.y, 0));
}

void character_create(GameObjectManager* game_object_manager_ptr, GpuDevice* in_gpu_device, Character* out_character)
{
	assert(out_character != NULL);

	const Vec3 character_root_position = vec3_new(0,75,150);

	GameObjectHandle root_object_handle			= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle body_object_handle			= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle head_object_handle			= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle legs_object_handle			= ADD_OBJECT(game_object_manager_ptr);	
	GameObjectHandle left_arm_object_handle		= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle right_arm_object_handle	= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle camera_root_object_handle	= ADD_OBJECT(game_object_manager_ptr);
	GameObjectHandle camera_object_handle		= ADD_OBJECT(game_object_manager_ptr);
	{	// Set up Main character hierarchy
		TransformComponent root_transform = {
			.trs = {
				.scale = vec3_new(5,5,5),
				.rotation = quat_identity,
				.translation = character_root_position,
			},
			.parent = {},
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, root_object_handle, root_transform);
		const Vec3 collider_half_extents = vec3_mul_componentwise(vec3_new(4.5f, 8.0f, 4.5f), root_transform.trs.scale);
		const PhysicsBody collider = {
			.position = character_collider_center(root_transform.trs),
			.orientation = quat_identity,
			.shape = { .type = SHAPE_TYPE_BOX, .box = box_shape_create(collider_half_extents) },
			.inverse_mass = 0.0f,
			.elasticity = 0.0f,
			.friction = 0.8f,
		};
		OBJECT_CREATE_COMPONENT(PhysicsBodyComponent, game_object_manager_ptr, root_object_handle, &collider);

		PlayerControlComponent player_control = {
			.move_speed = 200.0f,
		};
		OBJECT_CREATE_COMPONENT(PlayerControlComponent, game_object_manager_ptr, root_object_handle, player_control);

		StaticModel body_static_model;
		assert(static_model_load("data/meshes/StarterMech_Body.glb", in_gpu_device, &body_static_model));
		StaticModelComponent body_model_component_data = {.static_model = body_static_model, };
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, body_object_handle, body_model_component_data);

		TransformComponent body_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(0,0,0),
			},
			.parent = optional_init({
				.object_handle = root_object_handle,
				.name = optional_none(),	
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, body_object_handle, body_transform);

		StaticModel head_static_model;
		assert(static_model_load("data/meshes/StarterMech_Head.glb", in_gpu_device, &head_static_model));
		StaticModelComponent head_model_component_data = {.static_model = head_static_model, };
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, head_object_handle, head_model_component_data);

		TransformComponent head_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(0,2.5,0),
			},
			.parent = optional_init({
				.object_handle = body_object_handle,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, head_object_handle, head_transform);

		StaticModel arm_static_model;
		assert(static_model_load("data/meshes/StarterMech_Arm.glb", in_gpu_device, &arm_static_model));
		StaticModelComponent arm_model_component_data = {.static_model = arm_static_model, };
		
		// Left Arm
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, left_arm_object_handle, arm_model_component_data);

		TransformComponent left_arm_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(3.25,0,0),
			},
			.parent = optional_init({
				.object_handle = body_object_handle,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, left_arm_object_handle, left_arm_transform);

		// Right Arm
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, right_arm_object_handle, arm_model_component_data);

		TransformComponent right_arm_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(-3.25,0,0),
			},
			.parent = optional_init({
				.object_handle = body_object_handle,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, right_arm_object_handle, right_arm_transform);

		StaticModel legs_static_model;
		assert(static_model_load("data/meshes/StarterMech_Legs.glb", in_gpu_device, &legs_static_model));
		StaticModelComponent legs_model_component_data = {.static_model = legs_static_model, };
		OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, legs_object_handle, legs_model_component_data);

		TransformComponent legs_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(0,-4.5,0),
			},
			.parent = optional_init({
				.object_handle = body_object_handle,
				.ignore_rotation = true,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, legs_object_handle, legs_transform);

		TransformComponent cam_root_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_new(vec3_new(1,0,0), -15.0f * DEGREES_TO_RADIANS),
				.translation = character_root_position,
			},
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, camera_root_object_handle, cam_root_transform);

		// Camera Setup
		TransformComponent cam_transform = {
			.trs = {
				.scale = vec3_new(1,1,1),
				.rotation = quat_identity,
				.translation = vec3_new(0,0,200),
			},
			.parent = optional_init({
				.object_handle = camera_root_object_handle,
				.name = optional_none(),
			}),
		};
		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, camera_object_handle, cam_transform);
		CameraComponent cam_component_data = {
			.camera_up = vec3_new(0,1,0),
			.camera_forward = vec3_new(0,0,1),
			.fov = 60.0f,
		};
		OBJECT_CREATE_COMPONENT(CameraComponent, game_object_manager_ptr, camera_object_handle, cam_component_data);
	}

	//FCS TODO: Leaking the StaticModels by not adding them to this struct and cleaning them up later
	*out_character = (Character) {
		.root_object_handle	= root_object_handle,
		.body_object_handle	= body_object_handle,
		.head_object_handle = head_object_handle,
		.legs_object_handle = legs_object_handle,
		.left_arm_object_handle	= left_arm_object_handle,
		.right_arm_object_handle = right_arm_object_handle,
		.camera_root_object_handle = camera_root_object_handle,
		.camera_object_handle =	camera_object_handle,
	};
}

typedef struct CharacterUpdateData
{
	Character* character;

	Window* window;
	GameObjectManager* game_object_manager;

	f32 delta_time;
	Vec2 mouse_delta;

} CharacterUpdateData;

void camera_apply_rotation_clamped(Quat* q, f32 yaw_delta, f32 pitch_delta, f32 pitch_limit_degrees)
{
	f32 pitch_limit_rad = pitch_limit_degrees * DEGREES_TO_RADIANS;

    // Extract current pitch
    Vec3 forward = quat_rotate_vec3(*q, vec3_new(0,0,-1)); // engine's forward
    f32 current_pitch = asinf(CLAMP(forward.y, -1.0f, 1.0f)); // pitch relative to horizontal plane

    // Apply yaw freely around world up
    if (!f32_nearly_zero(yaw_delta))
    {
        Quat q_yaw = quat_new(vec3_new(0,1,0), yaw_delta);
        *q = quat_mul(q_yaw, *q);
    }

	f32 allowed_delta = pitch_delta;

	// Clamp to upper limit
	if (current_pitch + pitch_delta > pitch_limit_rad)
	{
		allowed_delta = pitch_limit_rad - current_pitch;
	}

	// Clamp to lower limit
	if (current_pitch + pitch_delta < -pitch_limit_rad)
	{
		allowed_delta = -pitch_limit_rad - current_pitch;
	}

	// Apply rotation if nonzero
	if (!f32_nearly_zero(allowed_delta))
	{
		Vec3 right = quat_rotate_vec3(*q, vec3_new(1,0,0));
		Quat q_pitch = quat_new(right, allowed_delta);
		*q = quat_mul(q_pitch, *q);
	}
}

void character_update(CharacterUpdateData* in_update_data)
{
	Character* character = in_update_data->character;
	Window* window = in_update_data->window;
	GameObjectManager* game_object_manager = in_update_data->game_object_manager;
	f32 delta_time = in_update_data->delta_time;
	Vec2 mouse_delta = in_update_data->mouse_delta;

	{   // Player Movement
		CameraComponent* cam_component = OBJECT_GET_COMPONENT(CameraComponent, game_object_manager, character->camera_object_handle);
		assert(cam_component);
		TransformComponent* root_transform = OBJECT_GET_COMPONENT(TransformComponent, game_object_manager, character->root_object_handle);
		PlayerControlComponent* player_control = OBJECT_GET_COMPONENT(PlayerControlComponent, game_object_manager, character->root_object_handle);
		assert(root_transform && player_control);
		const Vec3 old_translation = root_transform->trs.translation; 
		Vec3 up_vec = vec3_new(0,1,0);
		Vec3 forward_vec = vec3_normalize(vec3_plane_projection(cam_component->camera_forward, vec3_new(0,1,0)));
		Vec3 right_vec = vec3_cross(up_vec, forward_vec);

		Vec3 move_vec = vec3_zero;
		if (window_input_pressed(window, 'W'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(forward_vec, player_control->move_speed ));
		}
		if (window_input_pressed(window, 'S'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(forward_vec, -player_control->move_speed ));
		}
		if (window_input_pressed(window, 'A'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(right_vec, player_control->move_speed ));
		}
		if (window_input_pressed(window, 'D'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(right_vec, -player_control->move_speed ));
		}
		if (window_input_pressed(window, 'E') || window_input_pressed(window, KEY_SPACE))
		{
			move_vec = vec3_add(move_vec, vec3_scale(up_vec, player_control->move_speed ));
		}
		if (window_input_pressed(window, 'Q'))
		{
			move_vec = vec3_add(move_vec, vec3_scale(up_vec, -player_control->move_speed ));
		}

		if (window_input_pressed(window, KEY_SHIFT))
		{
			move_vec = vec3_scale(move_vec, 2.5f);
		}

		// Finally scale everything by delta time
		move_vec = vec3_scale(move_vec, delta_time);

		{
			const f32 body_rot_lerp_speed = 10.0 * delta_time;

			root_transform->trs.translation = vec3_add(old_translation, move_vec);
			Quat old_body_rotation = root_transform->trs.rotation;
			Quat new_body_rotation = mat4_to_quat(mat4_from_axes(forward_vec, up_vec));
			root_transform->trs.rotation = quat_slerp(body_rot_lerp_speed, old_body_rotation, new_body_rotation);
		}

		TransformComponent* cam_root_transform = OBJECT_GET_COMPONENT(TransformComponent, game_object_manager, character->camera_root_object_handle);
		assert(cam_root_transform);

		//FCS TODO: Lerp for lazy cam
		cam_root_transform->trs.translation = root_transform->trs.translation;

		TransformComponent* legs_transform = OBJECT_GET_COMPONENT(TransformComponent, game_object_manager, character->legs_object_handle);
		assert(legs_transform);


		if (!vec3_nearly_equal(vec3_zero, move_vec)) 
		{
			const Vec3 legs_up = vec3_new(0,1,0);
			const f32 legs_up_dot_move_vec = fabsf(vec3_dot(legs_up, vec3_normalize(move_vec)));
			if (!f32_nearly_equal(legs_up_dot_move_vec, 1.0f))
			{
				const f32 legs_rot_lerp_speed = 10.0 * delta_time;
				const Quat old_rotation = legs_transform->trs.rotation;
				const Quat desired_rotation = mat4_to_quat(mat4_from_axes(vec3_normalize(move_vec), legs_up));
				legs_transform->trs.rotation = quat_slerp(legs_rot_lerp_speed, old_rotation, desired_rotation);
			}
		}
	}

	{	// Camera Control
		// currently assumes cam root has no parent (we manually move it into place)
		TransformComponent* cam_root_transform = OBJECT_GET_COMPONENT(TransformComponent, game_object_manager, character->camera_root_object_handle);
		assert(cam_root_transform);

		const f32 cam_rotation_speed = 1.0f;

		if (!f32_nearly_zero(mouse_delta.x) || !f32_nearly_zero(mouse_delta.y))
		{
			const Vec2 camera_move_speed = vec2_scale(mouse_delta, -1.0f * cam_rotation_speed * delta_time);

			camera_apply_rotation_clamped(
				&cam_root_transform->trs.rotation, 
				camera_move_speed.x,
				camera_move_speed.y,
				80.0f 
			);
		}
	}
}

// FCS TODO:
// need to n-buffer resources for frames in flight... (joint matrices still left to do)

static void spawn_box_chain(GameObjectManager* manager, Vec3 origin, bool lock_orientation)
{
	const Vec3 colors[] = {{0,0,1}, {1,0,1}, {0,1,1}, {1,1,0}};
	const Shape shape = {.type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(5, 5, 5))};
	PhysicsScene* scene = &manager->physics_scene;
	PhysicsBody* links[ARRAY_COUNT(colors)];
	for (i32 i = 0; i < ARRAY_COUNT(colors); ++i)
	{
		links[i] = game_object_spawn_physics_body(manager, &(PhysicsBody){
			.position = vec3_add(origin, vec3_new(20.0f * i, 0, 0)),
			.orientation = quat_identity,
			.shape = shape,
			.inverse_mass = i == 0 ? 0.0f : 1.0f,
			.elasticity = 1.0f,
			.friction = 0.5f,
			.debug_color = lock_orientation ? vec3_new(1.0f, 0.5f, 0.1f) : colors[i],
		});
	}

	const i32 first_distance_link = lock_orientation ? 2 : 1;
	for (i32 i = first_distance_link; i < ARRAY_COUNT(links); ++i)
	{
		PhysicsBody* previous = links[i - 1];
		PhysicsBody* link = links[i];
		const Vec3 pivot = previous->position;
		PhysicsConstraint distance = physics_constraint_distance_init(scene);
		distance.body_a = previous;
		distance.body_b = link;
		distance.anchor_a = physics_body_world_to_local_space(previous, pivot);
		distance.anchor_b = physics_body_world_to_local_space(link, pivot);
		physics_scene_add_constraint(scene, &distance);
		if (lock_orientation)
		{
			PhysicsConstraint lock = physics_constraint_orientation_init(previous, link);
			physics_scene_add_constraint(scene, &lock);
		}
	}

	// Solve the root stop after the outer links have loaded it on each pass.
	if (lock_orientation)
	{
		PhysicsConstraint hinge = physics_constraint_limited_hinge_init(links[0], links[1],
			origin, vec3_new(0, 0, 1), degrees_to_radians(-75), degrees_to_radians(45));
		physics_scene_add_constraint(scene, &hinge);
	}
}

static void spawn_shoulder_demo(GameObjectManager* manager)
{
	const Vec3 pivot = vec3_new(180, 120, 0);
	PhysicsBody* mount = game_object_spawn_physics_body(manager, &(PhysicsBody){
		.position = pivot, .orientation = quat_identity,
		.shape = {.type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(3, 3, 3))},
		.debug_color = vec3_new(0.15f, 0.65f, 0.55f),
	});
	const Vec3 arm_offset = vec3_new(16, 0, 0);
	const Vec3 spin = vec3_new(0, 1.5f, 0);
	PhysicsBody* arm = game_object_spawn_physics_body(manager, &(PhysicsBody){
		.position = vec3_add(pivot, arm_offset), .orientation = quat_identity,
		.linear_velocity = vec3_cross(spin, arm_offset), .angular_velocity = spin,
		.inverse_mass = 1, .friction = 0.5f,
		.shape = {.type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(9, 3, 3))},
		.debug_color = vec3_new(0.25f, 1.0f, 0.8f),
	});
	PhysicsConstraint shoulder = physics_constraint_limited_cv_init(mount, arm, pivot,
		vec3_new(1, 0, 0), vec3_new(0, 1, 0), vec2_new(degrees_to_radians(35), degrees_to_radians(60)));
	physics_scene_add_constraint(&manager->physics_scene, &shoulder);
}

static PhysicsBody* spawn_ragdoll_box(GameObjectManager* manager, Vec3 center, Quat orientation,
	Vec3 half_extents, f32 inverse_mass, Vec3 color)
{
	return game_object_spawn_physics_body(manager, &(PhysicsBody){
		.position = center, .orientation = orientation,
		.shape = {.type = SHAPE_TYPE_BOX, .box = box_shape_create(half_extents)},
		.inverse_mass = inverse_mass, .friction = 0.8f, .debug_color = color,
	});
}

static void spawn_ragdoll_demo(GameObjectManager* manager, Vec3 origin)
{
	PhysicsScene* scene = &manager->physics_scene;
	// Tilt the whole starting pose so one foot hits first and the joints take a load.
	const Quat pose = quat_new(vec3_new(1, 0, 0.4f), degrees_to_radians(18));
	const Vec3 across = quat_rotate_vec3(pose, vec3_new(1, 0, 0));
	const Vec3 up = quat_rotate_vec3(pose, vec3_new(0, 1, 0));
	PhysicsBody* torso = spawn_ragdoll_box(manager, origin, pose,
		vec3_new(10, 16, 6), 0.5f, vec3_new(0.55f, 0.28f, 0.85f));
	PhysicsBody* head = spawn_ragdoll_box(manager,
		physics_body_local_to_world_space(torso, vec3_new(0, 27, 0)), pose,
		vec3_new(6.8f, 6.8f, 6.8f), 2.0f, vec3_new(1.0f, 0.82f, 0.57f));
	const Vec3 neck_pivot = physics_body_local_to_world_space(torso, vec3_new(0, 19, 0));
	PhysicsConstraint neck = physics_constraint_limited_hinge_init(torso, head, neck_pivot, across,
		degrees_to_radians(-30), degrees_to_radians(30));
	physics_scene_add_constraint(scene, &neck);

	for (i32 side = -1; side <= 1; side += 2)
	{
		PhysicsBody* upper_arm = spawn_ragdoll_box(manager,
			physics_body_local_to_world_space(torso, vec3_new(side * 21, 10, 0)), pose,
			vec3_new(6, 3.6f, 3.6f), 1.8f, vec3_new(0.78f, 0.56f, 1.0f));
		PhysicsBody* forearm = spawn_ragdoll_box(manager,
			physics_body_local_to_world_space(torso, vec3_new(side * 35, 10, 0)), pose,
			vec3_new(6, 3.6f, 3.6f), 2.25f, vec3_new(0.88f, 0.72f, 1.0f));
		const Vec3 shoulder_pivot = physics_body_local_to_world_space(torso, vec3_new(side * 13.6f, 10, 0));
		PhysicsConstraint shoulder = physics_constraint_limited_cv_init(torso, upper_arm, shoulder_pivot,
			vec3_scale(across, side), up, vec2_new(degrees_to_radians(55), degrees_to_radians(70)));
		physics_scene_add_constraint(scene, &shoulder);

		// Put the elbow on the front edge so the forearm can fold forward past 90 degrees.
		const Vec3 elbow_pivot = physics_body_local_to_world_space(torso, vec3_new(side * 28, 10, 3.6f));
		PhysicsConstraint elbow = physics_constraint_limited_hinge_init(upper_arm, forearm,
			elbow_pivot, vec3_scale(up, -side), 0, degrees_to_radians(135));
		physics_scene_add_constraint(scene, &elbow);

		const f32 leg_x = side * 6.6f;
		PhysicsBody* thigh = spawn_ragdoll_box(manager,
			physics_body_local_to_world_space(torso, vec3_new(leg_x, -29.5f, 0)), pose,
			vec3_new(4, 7.5f, 4), 1.8f, vec3_new(0.37f, 0.4f, 0.83f));
		PhysicsBody* shin = spawn_ragdoll_box(manager,
			physics_body_local_to_world_space(torso, vec3_new(leg_x, -46.5f, 0)), pose,
			vec3_new(4, 7.5f, 4), 2.25f, vec3_new(0.52f, 0.57f, 0.94f));
		const Vec3 hip_pivot = physics_body_local_to_world_space(torso, vec3_new(leg_x, -19, 0));
		PhysicsConstraint hip = physics_constraint_limited_hinge_init(torso, thigh, hip_pivot, across,
			degrees_to_radians(-55), degrees_to_radians(40));
		physics_scene_add_constraint(scene, &hip);
		const Vec3 knee_pivot = physics_body_local_to_world_space(torso, vec3_new(leg_x, -38, -4));
		PhysicsConstraint knee = physics_constraint_limited_hinge_init(thigh, shin,
			knee_pivot, across, 0, degrees_to_radians(125));
		physics_scene_add_constraint(scene, &knee);
	}
}

static void spawn_motor_demo(GameObjectManager* manager, Vec3 base)
{
	const Vec3 axle = vec3_add(base, vec3_new(0, 27, 5));
	PhysicsBody* stand = game_object_spawn_physics_body(manager, &(PhysicsBody){
		.position = vec3_add(base, vec3_new(0, 14, 0)), .orientation = quat_identity,
		.shape = {.type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(3, 14, 2))},
		.debug_color = vec3_new(0.3f, 0.35f, 0.4f),
	});
	// The offset center of mass loads the motor even after the ball leaves the paddle.
	PhysicsBody* paddle = game_object_spawn_physics_body(manager, &(PhysicsBody){
		.position = vec3_add(axle, vec3_new(4, 0, 0)), .orientation = quat_identity,
		.shape = {.type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(16, 2, 2))},
		.inverse_mass = 0.2f, .elasticity = 0.2f, .friction = 0.6f,
		.debug_color = vec3_new(0.95f, 0.25f, 0.1f),
	});
	PhysicsConstraint motor = physics_constraint_motor_init(stand, paddle, axle, vec3_new(0, 0, 1), 2.0f);
	physics_scene_add_constraint(&manager->physics_scene, &motor);
	game_object_spawn_physics_body(manager, &(PhysicsBody){
		.position = vec3_add(axle, vec3_new(12, 14, 0)), .orientation = quat_identity,
		.shape = {.type = SHAPE_TYPE_SPHERE, .sphere = {.radius = 3}},
		.inverse_mass = 1, .elasticity = 0.5f, .friction = 0.6f,
		.debug_color = vec3_new(1.0f, 0.85f, 0.2f),
	});
}

static void spawn_platform_demo(GameObjectManager* manager, Vec3 start)
{
	game_object_spawn_physics_body(manager, &(PhysicsBody){
		.position = start, .orientation = quat_identity,
		.shape = {.type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(14, 2, 10))},
		.inverse_mass = 0, .friction = 1.0f,
		.mover = {.travel = vec3_new(32, 0, 0), .period = 10.0f},
		.debug_color = vec3_new(0.15f, 0.42f, 0.85f),
	});
	game_object_spawn_physics_body(manager, &(PhysicsBody){
		.position = vec3_add(start, vec3_new(0, 5, 0)), .orientation = quat_identity,
		.shape = {.type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(4, 3, 4))},
		.inverse_mass = 1.0f, .friction = 0.95f,
		.debug_color = vec3_new(0.95f, 0.65f, 0.2f),
	});
}

int main()
{
	// Init multithreaded task system
	TaskSystem task_system;
	task_system_init(&task_system);

	// Create our window	
	String window_title = string_new("C Game (");
	string_append(&window_title, gpu_get_api_name());
	string_append(&window_title, ")");

	i32 window_width = 1280;
	i32 window_height = 720;
    Window window = window_create(window_title.data, window_width, window_height);

	string_free(&window_title);

	GpuDevice gpu_device;
	gpu_create_device(&window, &gpu_device);
	const u32 swapchain_count = gpu_get_swapchain_count(&gpu_device);

	// Game objects and physics
	GameObjectManager game_object_manager = {};
	GameObjectManager* game_object_manager_ptr = &game_object_manager;
	game_object_manager_init(game_object_manager_ptr);

	PhysicsScene* physics_scene = &game_object_manager.physics_scene;

	{	// Add lots of bodies
		const i32 sqrt_iter_count = 3;
		for (i32 x = 0; x < sqrt_iter_count; ++x)
		{
			for (i32 z = 0; z < sqrt_iter_count; ++z)
			{
				const f32 spacing = 50.0f;
				const f32 pos_x = (x - sqrt_iter_count/4) * spacing; 
				const f32 pos_z = (z - sqrt_iter_count/4) * spacing;

				{	// Spheres

					const f32 radius = 5.f;
					game_object_spawn_physics_body(game_object_manager_ptr, &(PhysicsBody) {
						.position = vec3_new(pos_x,50.f,pos_z),
						.orientation = quat_identity,
						.linear_velocity = vec3_new(0,0,0),
						.shape = {
							.type = SHAPE_TYPE_SPHERE,
							.sphere = {
								.radius = radius,
							},
						},
						.inverse_mass = 1.f,
						.elasticity = 0.5f,
						.friction = 0.5f,
						.debug_color = vec3_new(1,1,0),
					});
				}

				{	// Boxes	
					const f32 w = 5;
					const f32 h = 5;
					const f32 d = 5;

					game_object_spawn_physics_body(game_object_manager_ptr, &(PhysicsBody) {
						.position = vec3_new(pos_x,60.f,pos_z),
						.orientation = quat_identity,
						.linear_velocity = vec3_zero,
						.shape = {
							.type = SHAPE_TYPE_BOX,
							.box = box_shape_create(vec3_new(w,h,d)),
						},
						.inverse_mass = 1.f,
						.elasticity = 0.25f,
						.friction = 0.5f,
						.debug_color = vec3_new(1,1,0),
					});
				}
			}
		}
	}

	{
		game_object_spawn_physics_body(game_object_manager_ptr, &(PhysicsBody) {
			.position = vec3_new(0,20,0),
			.orientation = quat_identity,
			.linear_velocity = vec3_zero,
			.shape = {
				.type = SHAPE_TYPE_BOX,
				.box = box_shape_create(vec3_new(5,5,5)),
			},
			.inverse_mass = 1.f,
			.elasticity = 1.0f,
			.friction = 0.5f,
			.debug_color = vec3_new(1,0,0),
		});

		// Floor
		game_object_spawn_physics_body(game_object_manager_ptr, &(PhysicsBody) {
			.position = vec3_new(0,-50,0),
			.orientation = quat_identity,
			.linear_velocity = vec3_zero,
			.shape = {
				.type = SHAPE_TYPE_BOX,
				.box = box_shape_create(vec3_new(1000,50,1000)),
			},
			.inverse_mass = 0.f,
			.elasticity = 1.0f,
			.friction = 0.5f,
			.debug_color = vec3_new(0,1,0),
		});

		// Freestanding box stack beside the drop grid.
		const i32 boxes_per_side = 4;
		const f32 box_half_size = 5.0f;
		const f32 box_clearance = 0.005f;
		const f32 box_spacing = 2.0f * box_half_size + box_clearance;
		const f32 center_offset = (boxes_per_side - 1) * box_spacing * 0.5f;
		const Shape stack_box = {
			.type = SHAPE_TYPE_BOX,
			.box = box_shape_create(vec3_new(box_half_size, box_half_size, box_half_size)),
		};
		for (i32 level = 0; level < boxes_per_side; ++level)
		{
			const f32 height = box_half_size + level * box_spacing;
			for (i32 x = 0; x < boxes_per_side; ++x)
			{
				const f32 pos_x = -40.0f - center_offset + x * box_spacing;
				for (i32 z = 0; z < boxes_per_side; ++z)
				{
					const f32 pos_z = 35.0f - center_offset + z * box_spacing;
					const bool blue = (x + level + z) % 2 == 0;
					game_object_spawn_physics_body(game_object_manager_ptr, &(PhysicsBody) {
						.position = vec3_new(pos_x, height, pos_z),
						.orientation = quat_identity,
						.shape = stack_box,
						.inverse_mass = 1.0f,
						.elasticity = 0.0f,
						.friction = 0.8f,
						.debug_color = blue ? vec3_new(0.2f, 0.55f, 1.0f) : vec3_new(1.0f, 0.55f, 0.15f),
					});
				}
			}
		}

		spawn_box_chain(game_object_manager_ptr, vec3_new(0, 120, 0), false);
		spawn_box_chain(game_object_manager_ptr, vec3_new(90, 120, 0), true);
		spawn_shoulder_demo(game_object_manager_ptr);
		spawn_ragdoll_demo(game_object_manager_ptr, vec3_new(250, 62, 0));
		spawn_motor_demo(game_object_manager_ptr, vec3_new(345, 0, 0));
		spawn_platform_demo(game_object_manager_ptr, vec3_new(420, 9, 0));

		{
			// side length s
			const f32 s = 15.0f;
			const f32 a = s / (2.0f * sqrtf(2.0f));
			const Vec3 tetrahedron_points[] = 
			{
				vec3_new( s / 2.0f,  0.0f,      -a),
				vec3_new(-s / 2.0f,  0.0f,      -a),
				vec3_new( 0.0f,      s / 2.0f,   a),
				vec3_new( 0.0f,     -s / 2.0f,   a)
			};

			game_object_spawn_physics_body(game_object_manager_ptr, &(PhysicsBody) {
				.position = vec3_new(0,5,0),
				.orientation = quat_identity,
				.linear_velocity = vec3_zero,
				.shape = {
					.type = SHAPE_TYPE_CONVEX,
					.convex = convex_shape_create(
						tetrahedron_points, 
						ARRAY_COUNT(tetrahedron_points)
					),
				},
				.inverse_mass = 1.f,
				.elasticity = 0.75f,
				.friction = 0.5f,
				.debug_color = vec3_new(0,1,1),
			});

			const f32 w = 5;
			const f32 h = 5;
			const f32 d = 5;
			const Vec3 box_points[] =
			{
				vec3_new(-w,-h,d),
				vec3_new( w,-h,d),
				vec3_new(-w, h,d),
				vec3_new( w, h,d),

				vec3_new(-w,-h,-d),
				vec3_new( w,-h,-d),
				vec3_new(-w, h,-d),
				vec3_new( w, h,-d),
			};

			game_object_spawn_physics_body(game_object_manager_ptr, &(PhysicsBody) {
				.position = vec3_new(-50,50,0),
				.orientation = quat_identity,
				.linear_velocity = vec3_zero,
				.shape = {
					.type = SHAPE_TYPE_CONVEX,
					.convex = convex_shape_create(
						box_points, 
						ARRAY_COUNT(box_points)
					),
				},
				.inverse_mass = 1.f,
				.elasticity = 1.0f,
				.friction = 0.5f,
				.debug_color = vec3_new(0,1,1),
			});
		}
	}

	Character character;
	character_create(game_object_manager_ptr, &gpu_device, &character);

	// Set up Static Model
	StaticModel static_model;
	if (!static_model_load("data/meshes/Monkey.glb", &gpu_device, &static_model))
	{
		printf("Failed to Load Static Model\n");
		return 1;
	}

	// Set up Animated Model 
	AnimatedModel animated_model;
	if (!animated_model_load("data/meshes/cesium_man.glb", &gpu_device, &animated_model))
	//if (!animated_model_load("data/meshes/blender_simple.glb", &gpu_device, &animated_model))
	//if (!animated_model_load("data/meshes/running.glb", &gpu_device, &animated_model))
	{
		printf("Failed to Load Animated Model\n");
		return 1;
	}
	
	// Generate some random animated + static meshes
	const i32 objects_to_create = 1000;
	for (i32 i = 0; i < objects_to_create; ++i)
	{
		GameObjectHandle new_object_handle = ADD_OBJECT(game_object_manager_ptr);

		const bool create_animated_model = true; //(i % 2) != 0;
		if (create_animated_model)
		{
			GpuBufferCreateInfo joints_buffer_create_info = {
				.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
				.is_cpu_visible = true,
				.size = animated_model.joints_buffer_size,
				.data = NULL,
			};
			GpuBuffer joint_matrices_buffer;
			gpu_create_buffer(&gpu_device, &joints_buffer_create_info, &joint_matrices_buffer);

			AnimatedModelComponent animated_model_component_data = {
				.animated_model = animated_model,
				.joint_matrices_buffer = joint_matrices_buffer,
				.mapped_buffer_data = gpu_map_buffer(&gpu_device, &joint_matrices_buffer),
				.animation_rate = rand_f32(0.0001f, 5.0f),
			};
			OBJECT_CREATE_COMPONENT(AnimatedModelComponent, game_object_manager_ptr, new_object_handle, animated_model_component_data);
		}
		else
		{
			StaticModelComponent static_model_component_data = {
				.static_model = static_model,
			};
			OBJECT_CREATE_COMPONENT(StaticModelComponent, game_object_manager_ptr, new_object_handle, static_model_component_data);
		}

		const f32 spawn_scale = create_animated_model ? objects_to_create / 25.0f : objects_to_create / 100.0f;
		const Vec3 scale = vec3_new(spawn_scale, spawn_scale, spawn_scale);
		
		const f32 spawn_span = objects_to_create / 2.0f;
		Vec3 translation = vec3_new(
			rand_f32(-spawn_span, spawn_span),
			rand_f32(-spawn_span, spawn_span) + 500,
			rand_f32(-spawn_span, spawn_span)
		);

		translation = vec3_add(translation, vec3_new(0,0, -spawn_span * 2));

		const Quat rotation	=	create_animated_model 
							?	quat_identity
							:	quat_new(
									vec3_normalize(vec3_new(
										rand_f32(-1, 1),
										rand_f32(-1, 1),
										rand_f32(-1, 1)
									)), 
									rand_f32(-180.0f, 180.0f) * DEGREES_TO_RADIANS
								);

		TransformComponent t = {
			.trs = {
				.scale = scale,
				.rotation = rotation,
				.translation = translation, 
			},
			.parent = {},
		};


		OBJECT_CREATE_COMPONENT(TransformComponent, game_object_manager_ptr, new_object_handle, t);
	}

	// Create Per Object Bind Group Layout at set index 1
	GpuBindGroupLayoutCreateInfo per_object_bind_group_layout_create_info = {
		.index = 1,
		.num_bindings = 6,
		.bindings = (GpuResourceBinding[6]){
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX,	
			},
		},
	};
	GpuBindGroupLayout per_object_bind_group_layout;
	gpu_create_bind_group_layout(&gpu_device, &per_object_bind_group_layout_create_info, &per_object_bind_group_layout);

	// Finally set up our render data for all game objects...
	for (i64 obj_idx = 0; obj_idx < sb_count(game_object_manager.game_object_array); ++obj_idx)
	{
		GameObjectHandle object_handle = {.idx = obj_idx, };
		if (!OBJECT_IS_VALID(&game_object_manager, object_handle)) 
		{
			continue;
		}

		game_object_render_data_setup(
			&game_object_manager, 
			&gpu_device, 
			object_handle, 
			&per_object_bind_group_layout
		);
	}

	GpuShaderCreateInfo vertex_shader_create_info = {
		.filename = "bin/shaders/mesh_render.vert",
	};
	GpuShader geometry_vertex_shader;
	gpu_create_shader(&gpu_device, &vertex_shader_create_info, &geometry_vertex_shader);

	GpuShaderCreateInfo fragment_shader_create_info = {
		.filename = "bin/shaders/mesh_render.frag",
	};
	GpuShader geometry_fragment_shader;
	gpu_create_shader(&gpu_device, &fragment_shader_create_info, &geometry_fragment_shader);

	GlobalUniformStruct global_uniform_data = {
		.view = mat4_identity,
		.projection = mat4_identity,
		.eye = vec4_zero,
	};
	
	GpuBufferCreateInfo global_uniform_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(GlobalUniformStruct),
		.data = NULL,
	};

	// Create Bind Group Layout
	GpuBindGroupLayoutCreateInfo global_bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 1,
		.bindings = (GpuResourceBinding[1]){
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX | GPU_SHADER_STAGE_FRAGMENT,	
			},
		},
	};
	GpuBindGroupLayout global_bind_group_layout;
	gpu_create_bind_group_layout(&gpu_device, &global_bind_group_layout_create_info, &global_bind_group_layout);

	// Create Global Bind Group
	GpuBindGroupCreateInfo global_bind_group_create_info = {
		.layout = &global_bind_group_layout,
	};

	GpuBuffer global_uniform_buffers[swapchain_count];
	GpuBindGroup global_bind_groups[swapchain_count];
	for (i32 global_uniform_idx = 0; global_uniform_idx < swapchain_count; ++global_uniform_idx)
	{
		gpu_create_buffer(&gpu_device, &global_uniform_buffer_create_info, &global_uniform_buffers[global_uniform_idx]);
		gpu_create_bind_group(&gpu_device, &global_bind_group_create_info, &global_bind_groups[global_uniform_idx]);

		//  Write updates to our global bind group
		const GpuBindGroupUpdateInfo global_bind_group_update_info = {
			.bind_group = &global_bind_groups[global_uniform_idx],
			.num_writes = 1,
			.writes = (GpuResourceWrite[1]){
				{
					.binding_index = 0,
					.type = GPU_BINDING_TYPE_BUFFER,
					.buffer_binding = {
						.buffer = &global_uniform_buffers[global_uniform_idx],
					},
				},
			},
		};
		gpu_update_bind_group(&gpu_device, &global_bind_group_update_info);
	}

	GpuBindGroupLayout* geometry_pipeline_bind_group_layouts[] = { &global_bind_group_layout, &per_object_bind_group_layout };

	GpuRenderPipelineCreateInfo geometry_render_pipeline_create_info = {
		.vertex_shader = &geometry_vertex_shader,
		.fragment_shader = &geometry_fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(geometry_pipeline_bind_group_layouts),
		.bind_group_layouts = geometry_pipeline_bind_group_layouts,
		.depth_test_enabled = true,
	};
	GpuRenderPipeline geometry_render_pipeline;
	gpu_create_render_pipeline(&gpu_device, &geometry_render_pipeline_create_info, &geometry_render_pipeline);

	// Depth must match the backbuffer, which is in pixels rather than window points
	i32 framebuffer_width = 0;
	i32 framebuffer_height = 0;
	window_get_framebuffer_dimensions(&window, &framebuffer_width, &framebuffer_height);

	GpuTextureCreateInfo depth_texture_create_info = {
		.format = GPU_FORMAT_D32_SFLOAT,	
		.extent = {
			.width = framebuffer_width,
			.height = framebuffer_height,
			.depth = 1,
		},
		.usage = GPU_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT,
	};
	GpuTexture depth_texture;
	gpu_create_texture(&gpu_device, &depth_texture_create_info, &depth_texture);

	// FCS TODO: move gui.h into gui/ add gui_render.h to that directory for all gui-rendering code
    // BEGIN GUI SETUP
    GuiContext gui_context;
    gui_init(&gui_context);

	// Gui Vertex Buffer
    size_t max_gui_vertices_size = sizeof(GuiVert) * 10000;
	GpuBufferCreateInfo gui_vertex_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = max_gui_vertices_size,
		.data = NULL,
	};
	GpuBuffer gui_vertex_buffer;
	gpu_create_buffer(&gpu_device, &gui_vertex_buffer_create_info, &gui_vertex_buffer);

	// Gui Index Buffer
    size_t max_gui_indices_size = sizeof(u32) * 30000;
	GpuBufferCreateInfo gui_index_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = max_gui_indices_size,
		.data = NULL,
	};
	GpuBuffer gui_index_buffer;
	gpu_create_buffer(&gpu_device, &gui_index_buffer_create_info, &gui_index_buffer);

	// Slug glyph data, read by gui.frag: curves, band lists, and a glyph table indexed by codepoint
	const TTFFont* gui_ttf_font = &gui_context.default_font.ttf_font;
	GuiGlyph gui_glyphs[ARRAY_COUNT(gui_ttf_font->glyph_outlines)];
	for (u32 i = 0; i < ARRAY_COUNT(gui_glyphs); ++i)
	{
		const TTFGlyphOutline* outline = &gui_ttf_font->glyph_outlines[i];
		gui_glyphs[i] = (GuiGlyph){
			.bounds = vec4_new(outline->x_min, outline->y_min, outline->x_max, outline->y_max),
			.band_offset = outline->band_offset,
			.band_count = outline->band_count,
		};
	}

	// CPU visible because the Vulkan backend can only write to host-visible buffers
	GpuBufferCreateInfo gui_curve_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(TTFCurve) * sb_count(gui_ttf_font->curves),
		.data = gui_ttf_font->curves,
	};
	GpuBuffer gui_curve_buffer;
	gpu_create_buffer(&gpu_device, &gui_curve_buffer_create_info, &gui_curve_buffer);

	GpuBufferCreateInfo gui_band_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(u32) * sb_count(gui_ttf_font->band_data),
		.data = gui_ttf_font->band_data,
	};
	GpuBuffer gui_band_buffer;
	gpu_create_buffer(&gpu_device, &gui_band_buffer_create_info, &gui_band_buffer);

	GpuBufferCreateInfo gui_glyph_buffer_create_info = {
		.usage = GPU_BUFFER_USAGE_STORAGE_BUFFER,
		.is_cpu_visible = true,
		.size = sizeof(gui_glyphs),
		.data = gui_glyphs,
	};
	GpuBuffer gui_glyph_buffer;
	gpu_create_buffer(&gpu_device, &gui_glyph_buffer_create_info, &gui_glyph_buffer);

	// Create GUI Bind Group Layout, Bind Group, and Update it
	GpuBindGroupLayoutCreateInfo gui_bind_group_layout_create_info = {
		.index = 0,
		.num_bindings = 5,
		.bindings = (GpuResourceBinding[5]){
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX ,	
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_VERTEX ,	
			},		
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_FRAGMENT,
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_FRAGMENT,
			},
			{
				.type = GPU_BINDING_TYPE_BUFFER,
				.shader_stages = GPU_SHADER_STAGE_FRAGMENT,
			},
		},
	};
	GpuBindGroupLayout gui_bind_group_layout;
	gpu_create_bind_group_layout(&gpu_device, &gui_bind_group_layout_create_info, &gui_bind_group_layout);

	GpuBindGroupCreateInfo gui_bind_group_create_info = {
		.layout = &gui_bind_group_layout,
	};
	GpuBindGroup gui_bind_group;
	gpu_create_bind_group(&gpu_device, &gui_bind_group_create_info, &gui_bind_group);

	const GpuBindGroupUpdateInfo gui_bind_group_update_info = {
		.bind_group = &gui_bind_group,
		.num_writes = 5,
		.writes = (GpuResourceWrite[5]){
			{
				.binding_index = 0,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_index_buffer,
				},
			},
			{
				.binding_index = 1,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_vertex_buffer,
				},
			},
			{
				.binding_index = 2,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_curve_buffer,
				},
			},
			{
				.binding_index = 3,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_band_buffer,
				},
			},
			{
				.binding_index = 4,
				.type = GPU_BINDING_TYPE_BUFFER,
				.buffer_binding = {
					.buffer = &gui_glyph_buffer,
				},
			},
		},
	};
	gpu_update_bind_group(&gpu_device, &gui_bind_group_update_info);

	GpuShaderCreateInfo gui_vertex_shader_create_info = {
			.filename = "bin/shaders/gui.vert",
		};
	GpuShader gui_vertex_shader;
	gpu_create_shader(&gpu_device, &gui_vertex_shader_create_info, &gui_vertex_shader);

	GpuShaderCreateInfo gui_fragment_shader_create_info = {
			.filename = "bin/shaders/gui.frag",
		};
	GpuShader gui_fragment_shader;
	gpu_create_shader(&gpu_device, &gui_fragment_shader_create_info, &gui_fragment_shader);

	GpuBindGroupLayout* gui_pipeline_bind_group_layouts[] = { &gui_bind_group_layout };
	GpuRenderPipelineCreateInfo gui_render_pipeline_create_info = {
		.vertex_shader = &gui_vertex_shader,
		.fragment_shader = &gui_fragment_shader,
		.num_bind_group_layouts = ARRAY_COUNT(gui_pipeline_bind_group_layouts),
		.bind_group_layouts = gui_pipeline_bind_group_layouts,
		.depth_test_enabled = false,
	};
	GpuRenderPipeline gui_render_pipeline;
	gpu_create_render_pipeline(&gpu_device, &gui_render_pipeline_create_info, &gui_render_pipeline);

	gpu_destroy_shader(&gpu_device, &gui_vertex_shader);
	gpu_destroy_shader(&gpu_device, &gui_fragment_shader);

	// END GUI SETUP

	// BEGIN DEBUG DRAW SETUP
	DebugDrawContext debug_draw_context;
	debug_draw_init(&gpu_device, &debug_draw_context);
	// END DEBUG DRAW SETUP

	u64 time = time_now();
    u32 current_frame = 0;

    double accumulated_delta_time = 0.0f;
    size_t frames_rendered = 0;

    i32 mouse_x, mouse_y;
	window_get_mouse_pos(&window, &mouse_x, &mouse_y);

	bool show_mouse = true;
	window_show_mouse_cursor(&window, show_mouse);
	bool reset_key_was_down = false;
	const f64 max_physics_step = 1.0 / 60.0;
	const i32 max_physics_steps_per_frame = 3;

	GpuCommandBuffer command_buffers[swapchain_count];
	for (i32 command_buffer_idx = 0; command_buffer_idx < swapchain_count; ++command_buffer_idx)
	{
		gpu_create_command_buffer(&gpu_device, &command_buffers[command_buffer_idx]);
	}

	while (window_handle_messages(&window))
	{
		if (window_input_pressed(&window, KEY_ESCAPE))
		{
			break;
		}
	
        const u64 new_time = time_now();
        const double delta_time = time_seconds(new_time - time);
        time = new_time;

		// Drop excess game time after a stall so catch-up work cannot stall the next frame.
		const f64 simulation_dt = fmin(delta_time, max_physics_steps_per_frame * max_physics_step);

        accumulated_delta_time += delta_time;
        frames_rendered++;	

		// Mouse is in current window points, so the GUI's screen size must track resizes
		window_get_dimensions(&window, &window_width, &window_height);

		i32 old_mouse_x = mouse_x;
		i32 old_mouse_y = mouse_y;
        window_get_mouse_pos(&window, &mouse_x, &mouse_y);

		Vec2 mouse_pos = vec2_new(mouse_x, mouse_y);
		Vec2 old_mouse_pos = vec2_new(old_mouse_x, old_mouse_y);
		i32 mouse_delta_x, mouse_delta_y;
		window_get_mouse_delta(&window, &mouse_delta_x, &mouse_delta_y);
		Vec2 mouse_delta = {
			.x = mouse_delta_x,
			.y = mouse_delta_y,
		};

		// Mouse Lock
		static bool show_mouse_pressed_last_frame = false;
		const bool show_debug_ui_pressed = window_input_pressed(&window, 'I')
										&& window_input_pressed(&window, KEY_LEFT_CONTROL);

		if (show_debug_ui_pressed && !show_mouse_pressed_last_frame)
		{
			show_mouse = !show_mouse;
			window_show_mouse_cursor(&window, show_mouse);
			show_mouse_pressed_last_frame = true;
		}
		else if (!show_debug_ui_pressed)
		{
			show_mouse_pressed_last_frame = false;
		}

        // BEGIN Gui
		static f32 global_animation_rate = 1.0f;

		GuiFrameState gui_frame_state = {
			.screen_size = vec2_new(window_width, window_height),
			.mouse_pos = mouse_pos,
			.mouse_buttons = {
				window_input_pressed(&window, KEY_LEFT_MOUSE),
				window_input_pressed(&window, KEY_RIGHT_MOUSE),
				window_input_pressed(&window, KEY_MIDDLE_MOUSE)
			},
		};
		gui_begin_frame(&gui_context, gui_frame_state);
		debug_draw_begin_frame(&debug_draw_context, current_frame);

		const bool reset_key_down = window_input_pressed(&window, 'R');
		bool reset_simulation = reset_key_down && !reset_key_was_down;
		reset_key_was_down = reset_key_down;
		if (show_mouse && gui_button(&gui_context, "Reset simulation (R)", vec2_new(15, 50), vec2_new(270, 50)) == GUI_CLICKED)
		{
			reset_simulation = true;
		}

		const char* impact_label = physics_scene->recheck_impacts ? "Impact rechecks: on" : "Impact rechecks: off";
		if (show_mouse && gui_button(&gui_context, impact_label, vec2_new(15, 100), vec2_new(270, 50)) == GUI_CLICKED)
		{
			physics_scene->recheck_impacts = !physics_scene->recheck_impacts;
		}

		if (!show_mouse) 
		{
			int x,y;
			window_get_position(&window, &x, &y);

			int width, height;
			window_get_dimensions(&window, &width, &height);

			window_set_mouse_pos(&window, x + width/2, y + height/2);
		}

		if (!show_mouse)
		{
			character_update(&(CharacterUpdateData) {
				.character = &character,
				.window = &window,
				.game_object_manager = &game_object_manager,
				.delta_time = simulation_dt,
				.mouse_delta = mouse_delta,
			});
		}

		// Animation Update
		sbuffer(Task*) animation_tasks = NULL;

		//FCS TODO: Way of counting components by type in game_object.h, so then num_updates_per_task could equal total_components / num_task_threads
		//const i32 num_task_threads = task_system_num_threads(&task_system);
		const i32 num_updates_per_task = 50;
		AnimationUpdateTaskData* current_task_data = NULL;

		u64 anim_update_start_time = time_now();

		//ENABLE_MEMORY_LOGGING();
		MEMORY_LOG(NULL, printf("\n\n BEGIN FRAME \n \n"));

		for (i64 obj_idx = 0; obj_idx < sb_count(game_object_manager.game_object_array); ++obj_idx)
		{
			GameObjectHandle object_handle = {.idx = obj_idx, };
			if (!OBJECT_IS_VALID(&game_object_manager, object_handle)) 
			{
				continue;
			}

			AnimatedModelComponent* animated_model_component = OBJECT_GET_COMPONENT(AnimatedModelComponent, &game_object_manager, object_handle);
			if (!animated_model_component)
			{
				continue;
			}

			if (!current_task_data)
			{
				current_task_data = FCS_MEM_ALLOC_ZEROED(sizeof(AnimationUpdateTaskData));
				*current_task_data = (AnimationUpdateTaskData) {
					.animated_model = &animated_model,
					.delta_time = delta_time,
					.global_animation_rate = global_animation_rate,
					.components_to_update = NULL,
				};
			}

			sb_push(current_task_data->components_to_update, animated_model_component);

			const bool is_last_object = obj_idx == (sb_count(game_object_manager.game_object_array) - 1);
			const bool component_update_limit_reached = sb_count(current_task_data->components_to_update) > num_updates_per_task;
			const bool should_start_task = is_last_object || component_update_limit_reached;

			if (should_start_task)
			{
				const bool multithread = true;
				if (multithread)
				{
					TaskDesc animation_task_desc = {
						.task_function = animation_update_task,
						.argument = current_task_data,
					};
					Task* new_animation_task = task_system_add_task(&task_system, &animation_task_desc);
					sb_push(animation_tasks, new_animation_task);
				}
				else
				{
					animation_update_task(current_task_data);
				}

				current_task_data = NULL;
			}	
		}

		// Need to wait on animation update
		task_system_wait_tasks(&task_system, animation_tasks);

		MEMORY_LOG(NULL, printf("\n\nEND FRAME"));
		//DISABLE_MEMORY_LOGGING();
		//MEMORY_LOG_STATS();

		u64 anim_update_end_time = time_now();
		const double anim_update_time_ms = time_seconds(anim_update_end_time - anim_update_start_time) * 1000;

		// Update Physics scene
		PhysicsBody* character_collider = OBJECT_GET_COMPONENT(PhysicsBodyComponent, game_object_manager_ptr, character.root_object_handle)->body;
		const TRS character_root = game_object_compute_global_transform(&game_object_manager, character.root_object_handle);
		const Vec3 collider_target = character_collider_center(character_root);
		if (reset_simulation)
		{
			physics_scene_reset(physics_scene);
		}
		else
		{
			// Sweep from the previous pose so contacts see the player's velocity, including sprinting.
			character_collider->linear_velocity = simulation_dt > 0.0
				? vec3_scale(vec3_sub(collider_target, character_collider->position), 1.0 / simulation_dt)
				: vec3_zero;
			const i32 steps = (i32)ceil(simulation_dt / max_physics_step);
			if (steps > 0)
			{
				const f32 step_dt = (f32)(simulation_dt / steps);
				for (i32 i = 0; i < steps; ++i) { physics_scene_update(physics_scene, step_dt); }
			}
		}
		// Anchor the collider to the current player pose even on the reset frame.
		character_collider->position = collider_target;

		// GUI
		if (show_mouse)
		{
			static f32 f = 0.25f;
			gui_slider_f32(&gui_context, &f, vec2_new(0, 2), "My Slider", vec2_new(15, 0), vec2_new(270, 50));

			f32 top_right_ui_position_y = 5.f;

			{ // Rolling FPS, resets on click
				double average_delta_time = accumulated_delta_time / (double)frames_rendered;
				f32 average_fps = 1.0 / average_delta_time;

				char buffer[128];
				snprintf(buffer, sizeof(buffer), "FPS: %.1f", round(average_fps));
				const f32 horizontal_padding = 5.f;
				const f32 fps_button_size = 155.f;
				if (
					gui_button(
						&gui_context,
						buffer, 
						vec2_new(window_width - fps_button_size - horizontal_padding, top_right_ui_position_y),
						vec2_new(fps_button_size, 30)) == GUI_CLICKED
					)
				{
					accumulated_delta_time = 0.0f;
					frames_rendered = 0;
				}
			}

			top_right_ui_position_y += 35.f;

			{ // Print Anim Update Time on Screen
				char buffer[256];
				snprintf(buffer, sizeof(buffer), "Anim Update Time: %.6f ms", anim_update_time_ms);
				const f32 horizontal_padding = 5.f;
				const f32 button_size = 400.f;
				gui_button(
					&gui_context, 
					buffer, 
					vec2_new(window_width - button_size - horizontal_padding, top_right_ui_position_y), 
					vec2_new(button_size, 30)
				);
			}

			top_right_ui_position_y += 35.f;

			{ // Allocator Memory Usage (memory allocated with the functions/macros in the memory/ directory)
				i32 allocated_memory = get_allocated_memory();
				char buffer[512];
				snprintf(buffer, sizeof(buffer), "Alloc Mem Usage: %i (%.2f MiB)", allocated_memory, allocated_memory / 1024.0 / 1024.0);

				const f32 horizontal_padding = 5.f;
				const f32 button_size = 400.f;
				gui_button(
					&gui_context, 
					buffer, 
					vec2_new(window_width - button_size - horizontal_padding, top_right_ui_position_y), 
					vec2_new(button_size, 30)
				);
			}

			top_right_ui_position_y += 35.f;

			{ // Total Memory Usage (memory usage as reported by the os for this process)
				i64 total_memory = app_get_memory_usage();
				char buffer[512];
				snprintf(buffer, sizeof(buffer), "Total Mem Usage: %lli (%.2f MiB)", total_memory, total_memory / 1024.0 / 1024.0);
				//printf("Total Mem Usage: %lli (%.2f MiB)\n", total_memory, total_memory / 1024.0 / 1024.0);

				const f32 horizontal_padding = 5.f;
				const f32 button_size = 400.f;
				gui_button(
					&gui_context, 
					buffer, 
					vec2_new(window_width - button_size - horizontal_padding, top_right_ui_position_y), 
					vec2_new(button_size, 30)
				);
			}

			static GuiWindow gui_window_1 = {
				.name = "Window 1",
				.window_rect =
				{
					.position =
					{
						.x = 15,
						.y = 250,
					},
					.size =
					{
						.x = 400,
						.y = 400,
					},
					.z_order = 1,
				},
				.is_expanded = true,
				.is_open = true,
				.is_resizable = true,
				.is_movable = true,
			};

			static GuiWindow gui_window_2 = {
				.name = "Window 2",
				.window_rect =
				{
					.position =
					{
						.x = 430,
						.y = 250,
					},
					.size =
					{
						.x = 400,
						.y = 400,
					},
					.z_order = 2,
				},
				.is_expanded = true,
				.is_open = false,
				.is_resizable = true,
				.is_movable = true,
			};

			gui_window_begin(&gui_context, &gui_window_1);
			if (gui_window_button(&gui_context, &gui_window_1, gui_window_2.is_open ? "Hide Window 2" : "Show Window 2") ==
				GUI_CLICKED)
			{
				gui_window_2.is_open = !gui_window_2.is_open;
			}

			static bool show_bezier = false;
			if (gui_window_button(&gui_context, &gui_window_1, show_bezier ? "Hide Bezier" : "Show Bezier") == GUI_CLICKED)
			{
				show_bezier = !show_bezier;
			}

			gui_window_slider_f32(&gui_context, &gui_window_1, &global_animation_rate, vec2_new(-5.0, 5.0), "Anim Rate");

			for (u32 i = 0; i < 12; ++i)
			{
				char buffer[256];
				snprintf(buffer, sizeof(buffer), "Btn %u", i);
				if (gui_window_button(&gui_context, &gui_window_1, buffer) == GUI_CLICKED)
				{
					printf("Clicked %s\n", buffer);
				}
			}
			gui_window_end(&gui_context, &gui_window_1);

			gui_window_begin(&gui_context, &gui_window_2);
			if (gui_window_button(&gui_context, &gui_window_2, "Toggle Movable") == GUI_CLICKED)
			{
				gui_window_2.is_movable = !gui_window_2.is_movable;
			}
			if (gui_window_button(&gui_context, &gui_window_2, "Toggle Resizable") == GUI_CLICKED)
			{
				gui_window_2.is_resizable = !gui_window_2.is_resizable;
			}
			gui_window_end(&gui_context, &gui_window_2);

			if (show_bezier)
			{
				static Vec2 bezier_points[] = {
					{.x = 400, .y = 200},
					{.x = 600, .y = 200},
					{.x = 800, .y = 600},
					{.x = 1000, .y = 600},
				};
				gui_bezier(&gui_context, 4, bezier_points, 25, vec4_new(0.6, 0, 0, 1), 5.0f);

				// Bezier Controls
				for (u32 i = 0; i < 4; ++i)
				{
					const Vec2 window_size = gui_context.frame_state.screen_size;
					const Vec2 point_pos_screen_space = {
						.x = bezier_points[i].x * window_size.x,
						.y = bezier_points[i].y * window_size.y,
					};
					const Vec2 bezier_button_size = vec2_new(25, 25);
					if (gui_button(&gui_context, "", vec2_sub(bezier_points[i], vec2_scale(bezier_button_size, 0.5)),
					bezier_button_size) == GUI_HELD)
					{
						bezier_points[i] = vec2_add(bezier_points[i], mouse_delta);
					}
				}
			}

			GpuBufferWriteInfo gui_index_buffer_write_info = {
				.buffer = &gui_index_buffer,
				.size = sizeof(u32) * sb_count(gui_context.draw_data.indices),
				.data = gui_context.draw_data.indices,
			};
			gpu_write_buffer(&gpu_device, &gui_index_buffer_write_info);

			GpuBufferWriteInfo gui_vertex_buffer_write_info = {
				.buffer = &gui_vertex_buffer,
				.size = sizeof(GuiVert) * sb_count(gui_context.draw_data.vertices),
				.data = gui_context.draw_data.vertices,
			};
			gpu_write_buffer(&gpu_device, &gui_vertex_buffer_write_info);
		}

		{	// Update Global Uniforms...
			Mat4 root_transform = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, character.root_object_handle));
			Vec3 root_position = mat4_get_translation(root_transform);
			Mat4 cam_transform = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, character.camera_object_handle));
			Vec3 cam_position = mat4_get_translation(cam_transform);

			CameraComponent* cam_component = OBJECT_GET_COMPONENT(CameraComponent, &game_object_manager, character.camera_object_handle);
			assert(cam_component);
			cam_component->camera_forward = vec3_normalize(vec3_sub(root_position, cam_position));
			const Vec3 cam_target = vec3_add(cam_position, cam_component->camera_forward);
			Mat4 view = mat4_look_at(cam_position, cam_target, cam_component->camera_up);
			Mat4 proj = mat4_perspective(cam_component->fov, (f32)window_width / (f32)window_height, 0.01f, 4000.0f);

			global_uniform_data.view = view;
			global_uniform_data.projection = proj;

			memcpy(&global_uniform_data.eye, &cam_position, sizeof(f32) * 3);

			GpuBufferWriteInfo global_uniform_buffer_write_info = {
				.buffer = &global_uniform_buffers[current_frame],
				.size = sizeof(GlobalUniformStruct),
				.data = &global_uniform_data,
			};
			gpu_write_buffer(&gpu_device, &global_uniform_buffer_write_info);
		}

		// gpu_reset_command_buffer will wait on the command buffer if its still in-flight
		GpuCommandBuffer* command_buffer = &command_buffers[current_frame];
		gpu_reset_command_buffer(&gpu_device, command_buffer);

		GpuBackBuffer backbuffer;
		gpu_get_next_backbuffer(&gpu_device, command_buffer, &backbuffer);
		GpuTexture backbuffer_texture;
		gpu_backbuffer_get_texture(&backbuffer, &backbuffer_texture);

		// Geometry Pass
		{
			GpuColorAttachmentDescriptor geometry_pass_color_attachments[] = {
				{
					.texture = &backbuffer_texture, 
					.clear_color = {0.392f, 0.584f, 0.929f, 0.f},
					.load_action = GPU_LOAD_ACTION_CLEAR,
					.store_action = GPU_STORE_ACTION_STORE,
				},
			};

			GpuDepthAttachmentDescriptor geometry_pass_depth_attachment = {
				.texture = &depth_texture,
				.clear_depth = 1.0f,
				.load_action = GPU_LOAD_ACTION_CLEAR,
				.store_action = GPU_STORE_ACTION_STORE,
			};

			GpuRenderPassCreateInfo geometry_render_pass_create_info = {
				.num_color_attachments = ARRAY_COUNT(geometry_pass_color_attachments), 
				.color_attachments = geometry_pass_color_attachments,
				.depth_attachment = &geometry_pass_depth_attachment,
				.command_buffer = command_buffer,
			};
			GpuRenderPass geometry_render_pass;
			gpu_begin_render_pass(&gpu_device, &geometry_render_pass_create_info, &geometry_render_pass);
			gpu_render_pass_set_render_pipeline(&geometry_render_pass, &geometry_render_pipeline);
			gpu_render_pass_set_bind_group(&geometry_render_pass, &geometry_render_pipeline, &global_bind_groups[current_frame]);

			for (i64 obj_idx = 0; obj_idx < sb_count(game_object_manager.game_object_array); ++obj_idx)
			{
				GameObjectHandle object_handle = {.idx = obj_idx, };
				if (!OBJECT_IS_VALID(&game_object_manager, object_handle)) 
				{
					continue;
				}

				TransformComponent* transform_component = OBJECT_GET_COMPONENT(TransformComponent, &game_object_manager, object_handle);
				ObjectRenderDataComponent* render_data_component = OBJECT_GET_COMPONENT(ObjectRenderDataComponent, &game_object_manager, object_handle);
				if (transform_component && render_data_component)
				{
					StaticModelComponent* static_model_component = OBJECT_GET_COMPONENT(StaticModelComponent, &game_object_manager, object_handle);
					AnimatedModelComponent* animated_model_component = OBJECT_GET_COMPONENT(AnimatedModelComponent, &game_object_manager, object_handle);

					// Need one of these to draw a mesh
					if (!static_model_component && !animated_model_component)
					{
						continue;
					}

					//Assign to our persistently mapped storage
					render_data_component->uniform_data[current_frame]->model = trs_to_mat4(game_object_compute_global_transform(&game_object_manager, object_handle)); 
					gpu_render_pass_set_bind_group(&geometry_render_pass, &geometry_render_pipeline, &render_data_component->bind_groups[current_frame]);	

					if (static_model_component)
					{
						gpu_render_pass_draw(&geometry_render_pass, 0, static_model_component->static_model.num_indices);
					}
					else if (animated_model_component)
					{
						gpu_render_pass_draw(&geometry_render_pass, 0, animated_model_component->animated_model.num_indices);
					}
				}
			}
			gpu_end_render_pass(&geometry_render_pass);
		}

		// Render Physics Bodies
		{
			//FCS TODO: Need some directionality so we can see orientation/rotational changes on spheres...

			for (i32 body_idx = 0; body_idx < sb_count(physics_scene->bodies); ++body_idx)
			{
				const PhysicsBody* body = physics_scene->bodies[body_idx];
				if (body == character_collider) { continue; } // The mech mesh represents this body.
				switch (body->shape.type)
				{
					case SHAPE_TYPE_SPHERE: 
					{
						// Draw higher res sphere when radius is large enough
						const i32 latitudes_and_longitudes	
							= body->shape.sphere.radius > 500
							? 48
							: 12;

						debug_draw_sphere(&debug_draw_context, &(DebugDrawSphere){
							.center = body->position,
							.orientation = body->orientation,
							.radius = body->shape.sphere.radius,
							.latitudes = latitudes_and_longitudes,
							.longitudes = latitudes_and_longitudes,
							.color = vec4_from_vec3(body->debug_color, 1.0f),
							.draw_type = DEBUG_DRAW_TYPE_SOLID,
							.shade = true,
						});
						break;
					}
					case SHAPE_TYPE_BOX:
					{
						const BoxShape box = body->shape.box;
						const Bounds box_bounds = box.bounds;
						const Vec3 half_extents = bounds_get_half_extents(&box_bounds);

						debug_draw_box(&debug_draw_context, &(DebugDrawBox){
							.center = body->position,
							.orientation = body->orientation,
							.half_extents = half_extents,
							.color = vec4_from_vec3(body->debug_color, 1.0f),
							.draw_type = DEBUG_DRAW_TYPE_SOLID,
							.shade = true,
						});
						break;
					}
					case SHAPE_TYPE_CONVEX:
					{
						const ConvexShape* convex = &body->shape.convex;
						const ConvexHull* hull = &convex->hull;

						debug_draw_mesh(&debug_draw_context, &(DebugDrawMesh){
							.center = body->position,
							.orientation = body->orientation,
							.vertex_positions = hull->points,
							.num_vertex_positions = sb_count(hull->points),
							.indices = (i32*) hull->tris,
							.num_indices = sb_count(hull->tris) * 3,
							.color = vec4_from_vec3(body->debug_color, 1.0f),
							.draw_type = DEBUG_DRAW_TYPE_SOLID,
							.shade = true,
						});
						
						break;
					}
				}
			}	
		}

		// Debug Draw Pass
		{
			debug_draw_sphere(&debug_draw_context, &(DebugDrawSphere){
				.center = vec3_new(0,200,-1000),
				.orientation = quat_identity,
				.radius = 25,
				.latitudes = 12,
				.longitudes = 12,
				.color = vec4_new(1,0,0,1),
				.draw_type = DEBUG_DRAW_TYPE_WIREFRAME,
			});

			debug_draw_box(&debug_draw_context, &(DebugDrawBox){
				.center = vec3_new(50,200,-1000),
				.orientation = quat_new(vec3_new(0,1,0), 45.f * DEGREES_TO_RADIANS),
				.half_extents = vec3_new(10,50,5),
				.color = vec4_new(0,1,0,1),
				.draw_type = DEBUG_DRAW_TYPE_WIREFRAME,
				.shade = false,
			});

			debug_draw_end_frame(&debug_draw_context, &gpu_device, &global_uniform_data.view, &global_uniform_data.projection);

			DebugDrawRecordInfo debug_draw_record_info = {
				.gpu_device = &gpu_device,
				.command_buffer = command_buffer,
				.color_texture = &backbuffer_texture,
				.depth_texture = &depth_texture,
			};
			debug_draw_record_commands(&debug_draw_context, &debug_draw_record_info);
		}

		// GUI Pass
		{
			GpuColorAttachmentDescriptor gui_color_attachments[] = {
				{
					.texture = &backbuffer_texture, 
					.load_action = GPU_LOAD_ACTION_LOAD,
					.store_action = GPU_STORE_ACTION_STORE,
				},
			};

			GpuRenderPassCreateInfo gui_render_pass_create_info = {
				.num_color_attachments = ARRAY_COUNT(gui_color_attachments), 
				.color_attachments = gui_color_attachments,
				.depth_attachment = NULL, 
				.command_buffer = command_buffer,
			};
			GpuRenderPass gui_render_pass;
			gpu_begin_render_pass(&gpu_device, &gui_render_pass_create_info, &gui_render_pass);
			gpu_render_pass_set_render_pipeline(&gui_render_pass, &gui_render_pipeline);
			gpu_render_pass_set_bind_group(&gui_render_pass, &gui_render_pipeline, &gui_bind_group);

			gpu_render_pass_draw(&gui_render_pass, 0, sb_count(gui_context.draw_data.indices));		
			gpu_end_render_pass(&gui_render_pass);
		}

		gpu_present_backbuffer(&gpu_device, command_buffer, &backbuffer);
		gpu_commit_command_buffer(&gpu_device, command_buffer);

		current_frame = (current_frame + 1) % swapchain_count;
	}

	gpu_device_wait_idle(&gpu_device);

	// Cleanup
	static_model_free(&gpu_device, &static_model);

	gpu_destroy_texture(&gpu_device, &depth_texture);


	for (i32 swapchain_idx = 0; swapchain_idx < swapchain_count; ++swapchain_idx)
	{
		gpu_destroy_buffer(&gpu_device, &global_uniform_buffers[swapchain_idx]);
		gpu_destroy_bind_group(&gpu_device, &global_bind_groups[swapchain_idx]);
	}
	gpu_destroy_bind_group_layout(&gpu_device, &global_bind_group_layout);

    gui_shutdown(&gui_context);

	gpu_destroy_device(&gpu_device);

	game_object_manager_destroy(game_object_manager_ptr);

	task_system_shutdown(&task_system);

	return 0;
}

