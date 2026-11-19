
#include "math/basic_math.h"
#include "math/matrix.h"
#include "math/quat.h"
#include "math/conversions.h"
#include "math/trs.h"
#include "basic_types.h"
#include "stdio.h"
#include "stretchy_buffer.h"
#include "math/lcp.h"
#include "memory/arena.h"
#include "physics/physics.h"
#include <string.h>
#include "file_helpers.h"
#include "truetype.h"

bool test_mat3_inverse();
bool test_mat4_inverse();
bool test_mat4_decompose();
bool test_quat_mat_conversions();
bool test_quat_product_matrices();
bool test_stretchy_buffer();
bool test_matn_mul_matn();
bool test_matmn_mul_matmn();
bool test_matn_from_matmn();
bool test_lcp_op_begin_end();
bool test_rotating_box_sweep();
bool test_distance_joint_drift();
bool test_orientation_lock();
bool test_resting_contacts();
bool test_manifold_points();
bool test_manifold_warm_start();
bool test_manifold_lifetime();
bool test_joint_supported_by_contact();
bool test_contact_friction();
bool test_contact_lift_off_and_bounce();
bool test_kinematic_collider();
bool test_physics_scene_reset();
bool test_physics_scene_remove_body();
bool test_arena_create_destroy();
bool test_arena_alloc();
bool test_arena_multiple_allocs();
bool test_arena_exact_fill();
bool test_arena_oom_detection();
bool test_arena_oom_null_return();
bool test_arena_growth();
bool test_ttf_slug_coverage();
bool test_ttf_slug_bands();

int main()
{
	bool success = true;
	success &= test_mat3_inverse();
	success &= test_mat4_inverse();
	success &= test_mat4_decompose();
	success &= test_quat_mat_conversions();
	success &= test_quat_product_matrices();
	success &= test_stretchy_buffer();
	success &= test_matn_mul_matn();
	success &= test_matmn_mul_matmn();
	success &= test_matn_from_matmn();
	success &= test_lcp_op_begin_end();
	success &= test_rotating_box_sweep();
	success &= test_distance_joint_drift();
	success &= test_orientation_lock();
	success &= test_resting_contacts();
	success &= test_manifold_points();
	success &= test_manifold_warm_start();
	success &= test_manifold_lifetime();
	success &= test_joint_supported_by_contact();
	success &= test_contact_friction();
	success &= test_contact_lift_off_and_bounce();
	success &= test_kinematic_collider();
	success &= test_physics_scene_reset();
	success &= test_physics_scene_remove_body();
	success &= test_arena_create_destroy();
	success &= test_arena_alloc();
	success &= test_arena_multiple_allocs();
	success &= test_arena_exact_fill();
	success &= test_arena_oom_detection();
	success &= test_arena_oom_null_return();
	success &= test_arena_growth();
	success &= test_ttf_slug_coverage();
	success &= test_ttf_slug_bands();


	if (!success)
	{
		printf("TESTS FAILED\n");
		return 1;
	}

	printf("ALL TESTS PASSED!\n");
	return 0;
}

bool test_quat_product_matrices()
{
	printf("  test_quat_product_matrices... ");
	const Quat q = {1, 2, -3, 4};
	const Vec4 v = vec4_new(q.x, q.y, q.z, q.w);
	for (i32 i = 0; i < 4; ++i)
	{
		Vec4 axis = vec4_zero;
		axis.v[i] = 1.0f;
		const Quat p = {axis.x, axis.y, axis.z, axis.w};
		const Quat qp = quat_mul(q, p);
		const Quat pq = quat_mul(p, q);
		assert(vec4_nearly_equal(mat4_mul_vec4(quat_left_matrix(q), axis), vec4_new(qp.x, qp.y, qp.z, qp.w)));
		assert(vec4_nearly_equal(mat4_mul_vec4(quat_right_matrix(q), axis), vec4_new(pq.x, pq.y, pq.z, pq.w)));
		assert(vec4_nearly_equal(mat4_mul_vec4(quat_left_matrix(p), v), mat4_mul_vec4(quat_right_matrix(q), axis)));
	}
	printf("PASSED\n");
	return true;
}

bool test_mat3_inverse()
{
	printf("  test_mat3_inverse... ");

	Mat3 matrix = {
		.d = {
			12.0f, 2.0f, 1.0f,
			0.0f, 0.0f, 1.0f,
			0.0f, 1.0f, 5.0f
		},
	};

	optional(Mat3) inverse = mat3_inverse(matrix);
	assert(optional_is_set(inverse));

	Mat3 result = mat3_mul_mat3(matrix, optional_get(inverse));	
	for (i32 row = 0; row < 3; ++row)
	{
		for (i32 col = 0; col < 3; ++col)
		{
			assert(f32_nearly_equal(result.d[row][col], mat3_identity.d[row][col]));
		}
	}

	printf("PASSED\n");
	return true;
}

bool test_mat4_inverse()
{
	printf("  test_mat4_inverse... ");

	Mat4 matrix = {
		.d = {
			12.0f, 2.0f, 1.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 1.0f,
			0.0f, 1.0f, 5.0f, 1.0f,
			11.0f, 1.0f, 0.0f, 10.0f
		},
	};

	optional(Mat4) inverse = mat4_inverse(matrix);
	assert(optional_is_set(inverse));

	Mat4 result = mat4_mul_mat4(matrix, optional_get(inverse));	
	for (i32 row = 0; row < 4; ++row)
	{
		for (i32 col = 0; col < 4; ++col)
	  		{
	  			assert(f32_nearly_equal(result.d[row][col], mat4_identity.d[row][col]));
	  		}
	}

	printf("PASSED\n");
	return true;
}

bool test_mat4_decompose()
{
	printf("  test_mat4_decompose... ");

	Vec3 scale = vec3_new(1,2,3);
	Mat4 scale_matrix = mat4_scale(scale);

	Quat rotation = quat_new(vec3_new(1, 0, 0), 0.5);
	Mat4 rotation_matrix = quat_to_mat4(rotation);

	Vec3 translation = vec3_new(4,5,6);
	Mat4 translation_matrix = mat4_translation(translation);

	Mat4 original_matrix = mat4_mul_mat4(mat4_mul_mat4(scale_matrix, rotation_matrix), translation_matrix);
	TRS trs = mat4_to_trs(original_matrix);
	Mat4 recomposed_matrix = mat4_mul_mat4(mat4_mul_mat4(mat4_scale(trs.scale), quat_to_mat4(trs.rotation)), mat4_translation(trs.translation));

	bool result = 	vec3_nearly_equal(scale, trs.scale)
		&&	quat_nearly_equal(rotation, trs.rotation)
		&& 	vec3_nearly_equal(translation, trs.translation)
		&& 	mat4_nearly_equal(original_matrix, recomposed_matrix);

	printf("PASSED\n");
	return result;
}

bool test_quat_mat_conversions()
{
	printf("  test_quat_mat_conversions... ");

	Quat q1 = quat_normalize(quat_new(vec3_new(1, 1, 0), 0.5));
	Mat4 m1 = quat_to_mat4(q1);
	Quat q2 = mat4_to_quat(m1);
	Mat4 m2 = quat_to_mat4(q2);

	bool result = quat_nearly_equal(q1, q2) && mat4_nearly_equal(m1,m2);

	printf("PASSED\n");
	return result;
}

bool test_stretchy_buffer()
{
	printf("  test_stretchy_buffer... ");

	sbuffer(i32) my_stretchy_buffer = NULL;

	i32 my_array[4] = {
		1,
		2,
		3,
		4,
	};

	sb_append_array(my_stretchy_buffer, my_array, ARRAY_COUNT(my_array));

	if (sb_count(my_stretchy_buffer) != ARRAY_COUNT(my_array))
	{
		printf("sb_append_array not behaving as expected. Counts differ.\n");
		return false;
	}

	bool success = true;

	for (i32 i = 0; i < sb_count(my_stretchy_buffer); ++i)
	{
		if (my_stretchy_buffer[i] != my_array[i])
		{
			printf("sb_append_array not behaving as expected. Elements differ.\n");
			success = false;
			break;
		}
	}

	sbuffer(i32) new_stretchy_buffer = NULL;
	sb_copy(new_stretchy_buffer, my_stretchy_buffer);

	if (sb_count(new_stretchy_buffer) != sb_count(my_stretchy_buffer))
	{
		printf("sb_copy not behaving as expected. Counts differ.\n");
		return false;
	}


	for (i32 i = 0; i < sb_count(new_stretchy_buffer); ++i)
	{
		if (new_stretchy_buffer[i] != my_stretchy_buffer[i])
		{
			printf("sb_copy not behaving as expected. Elements differ.\n");
			success = false;
			break;
		}
	}

	sb_free(new_stretchy_buffer);
	sb_free(my_stretchy_buffer);

	if (success) printf("PASSED\n");
	return success;
}

bool test_matn_mul_matn()
{
	printf("  test_matn_mul_matn... ");

	Arena* arena = arena_create(&default_arena_desc);

    // A = [[1,2],[3,4]], B = [[5,6],[7,8]]
    // A*B = [[1*5+2*7, 1*6+2*8],[3*5+4*7, 3*6+4*8]]
    //      = [[19,22],[43,50]]
    MatN a = matn_new(arena, 2);
    a.rows[0].data[0] = 1.0f; a.rows[0].data[1] = 2.0f;
    a.rows[1].data[0] = 3.0f; a.rows[1].data[1] = 4.0f;

    MatN b = matn_new(arena, 2);
    b.rows[0].data[0] = 5.0f; b.rows[0].data[1] = 6.0f;
    b.rows[1].data[0] = 7.0f; b.rows[1].data[1] = 8.0f;

    MatN c = matn_mul_matn(arena, &a, &b);
    assert(f32_nearly_equal(c.rows[0].data[0], 19.0f));
    assert(f32_nearly_equal(c.rows[0].data[1], 22.0f));
    assert(f32_nearly_equal(c.rows[1].data[0], 43.0f));
    assert(f32_nearly_equal(c.rows[1].data[1], 50.0f));

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_arena_create_destroy()
{
	printf("  test_arena_create_destroy... ");

	ArenaDesc desc = { .size = 128 };
	Arena* arena = arena_create(&desc);
	assert(arena);
	assert(arena->total_size == 128);
	assert(arena->remaining_size == 128);
	assert(arena->start == arena->current);

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_arena_alloc()
{
	printf("  test_arena_alloc... ");

	ArenaDesc desc = { .size = 256 };
	Arena* arena = arena_create(&desc);

	void* a = arena_alloc(arena, 64);
	assert(a);
	assert(arena->remaining_size == 192);

	void* b = arena_alloc(arena, 32);
	assert(b);
	assert(arena->remaining_size == 160);

	u8* a_ptr = (u8*)a;
	u8* b_ptr = (u8*)b;
	assert(b_ptr == a_ptr + 64);

	for (u64 i = 0; i < 64; ++i) a_ptr[i] = (u8)(i & 0xFF);
	for (u64 i = 0; i < 32; ++i) b_ptr[i] = (u8)((i + 1) & 0xFF);
	assert(a_ptr[0] == 0);
	assert(a_ptr[63] == 63);
	assert(b_ptr[0] == 1);
	assert(b_ptr[31] == 32);

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_arena_multiple_allocs()
{
	printf("  test_arena_multiple_allocs... ");

	ArenaDesc desc = { .size = 1024 };
	Arena* arena = arena_create(&desc);

	u64 remaining = 1024;
	for (u64 i = 0; i < 10; ++i)
	{
		u64 sz = 16 + i * 8;
		void* p = arena_alloc(arena, sz);
		assert(p);
		remaining -= sz;
		assert(arena->remaining_size == remaining);
	}

	assert(arena->remaining_size == 1024 - (16*10 + 8 * 45));

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_arena_exact_fill()
{
	printf("  test_arena_exact_fill... ");

	ArenaDesc desc = { .size = 128 };
	Arena* arena = arena_create(&desc);

	void* a = arena_alloc(arena, 80);
	assert(a);
	assert(arena->remaining_size == 48);

	void* b = arena_alloc(arena, 48);
	assert(b);
	assert(arena->remaining_size == 0);

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_arena_oom_detection()
{
	printf("  test_arena_oom_detection... ");

	ArenaDesc desc = { .size = 64 };
	Arena* arena = arena_create(&desc);

	void* a = arena_alloc(arena, 40);
	assert(a);
	assert(arena->remaining_size == 24);

	bool would_oom = (50 > arena->remaining_size);
	assert(would_oom == true);

	void* b = arena_alloc(arena, 24);
	assert(b);
	assert(arena->remaining_size == 0);

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_arena_oom_null_return()
{
	printf("  test_arena_oom_null_return... ");

	ArenaDesc desc = { .size = 64 };
	Arena* arena = arena_create(&desc);

	void* a = arena_alloc(arena, 64);
	assert(a);
	assert(arena->remaining_size == 0);

	void* prev_current = arena->current;
	u64 prev_remaining = arena->remaining_size;

	void* b = arena_alloc(arena, 1);
	assert(b == NULL);
	assert(arena->current == prev_current);
	assert(arena->remaining_size == prev_remaining);

	void* c = arena_alloc(arena, 999);
	assert(c == NULL);
	assert(arena->current == prev_current);
	assert(arena->remaining_size == prev_remaining);

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_arena_growth()
{
	printf("  test_arena_growth... ");

	ArenaDesc desc = { .size = 64, .allow_growth = true };
	Arena* arena = arena_create(&desc);
	assert(arena->allow_growth == true);
	assert(arena->next == NULL);

	void* a = arena_alloc(arena, 64);
	assert(a);
	assert(arena->remaining_size == 0);

	void* b = arena_alloc(arena, 32);
	assert(b);
	assert(arena->next != NULL);
	assert(arena->next->remaining_size == 64 - 32);

	u8* a_ptr = (u8*)a;
	u8* b_ptr = (u8*)b;
	for (u64 i = 0; i < 64; ++i) a_ptr[i] = (u8)(i & 0xFF);
	for (u64 i = 0; i < 32; ++i) b_ptr[i] = (u8)((i + 100) & 0xFF);
	assert(a_ptr[0] == 0);
	assert(a_ptr[63] == 63);
	assert(b_ptr[0] == 100);
	assert(b_ptr[31] == 131);

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_matmn_mul_matmn()
{
	printf("  test_matmn_mul_matmn... ");

	Arena* arena = arena_create(&default_arena_desc);

	// A is 2x3, B is 3x2
	// A = [[1,2,3],[4,5,6]]
	// B = [[7,8],[9,10],[11,12]]
	// A*B = [[1*7+2*9+3*11, 1*8+2*10+3*12],[4*7+5*9+6*11, 4*8+5*10+6*12]]
	//      = [[58,64],[139,154]]
	MatMN a = matmn_new(arena, 2, 3);
	a.rows[0].data[0] = 1.0f; a.rows[0].data[1] = 2.0f; a.rows[0].data[2] = 3.0f;
	a.rows[1].data[0] = 4.0f; a.rows[1].data[1] = 5.0f; a.rows[1].data[2] = 6.0f;

	MatMN b = matmn_new(arena, 3, 2);
	b.rows[0].data[0] = 7.0f;  b.rows[0].data[1] = 8.0f;
	b.rows[1].data[0] = 9.0f;  b.rows[1].data[1] = 10.0f;
	b.rows[2].data[0] = 11.0f; b.rows[2].data[1] = 12.0f;

	MatMN c = matmn_mul_matmn(arena, &a, &b);
	assert(c.m == 2 && c.n == 2);
	assert(f32_nearly_equal(c.rows[0].data[0], 58.0f));
	assert(f32_nearly_equal(c.rows[0].data[1], 64.0f));
	assert(f32_nearly_equal(c.rows[1].data[0], 139.0f));
	assert(f32_nearly_equal(c.rows[1].data[1], 154.0f));

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_matn_from_matmn()
{
	printf("  test_matn_from_matmn... ");

	Arena* arena = arena_create(&default_arena_desc);

	// Build a 2x2 MatMN with known values and verify MatN copy is correct
	MatMN src = matmn_new(arena, 2, 2);
	src.rows[0].data[0] = 3.0f; src.rows[0].data[1] = 7.0f;
	src.rows[1].data[0] = 1.0f; src.rows[1].data[1] = 5.0f;

	MatN dst = matn_from_matmn(arena, &src);
	assert(matn_num_dims(&dst) == 2);
	assert(f32_nearly_equal(dst.rows[0].data[0], 3.0f));
	assert(f32_nearly_equal(dst.rows[0].data[1], 7.0f));
	assert(f32_nearly_equal(dst.rows[1].data[0], 1.0f));
	assert(f32_nearly_equal(dst.rows[1].data[1], 5.0f));

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

bool test_rotating_box_sweep()
{
	printf("  test_rotating_box_sweep... ");

	// The box starts below the sphere. Only its rotation can close the gap.
	PhysicsBody sphere = {
		.position = vec3_new(1.8f, 0.55f, 0.0f),
		.orientation = quat_identity,
		.shape = { .type = SHAPE_TYPE_SPHERE, .sphere = { .radius = 0.2f } },
	};
	const PhysicsBody box = {
		.orientation = quat_identity,
		.angular_velocity = vec3_new(0.0f, 0.0f, 1.0f),
		.inverse_mass = 1.0f,
		.shape = { .type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(2.0f, 0.2f, 0.2f)) },
	};
	const f32 dt = 0.2f;
	PhysicsBody box_as_b = box;
	PhysicsBody box_as_a = box;
	PhysicsContact ab = {}, ba = {};
	bool hit_ab = physics_bodies_intersect_dt(&sphere, &box_as_b, dt, &ab);
	bool hit_ba = physics_bodies_intersect_dt(&box_as_a, &sphere, dt, &ba);
	assert(hit_ab && hit_ba);
	assert(ab.time_of_impact > 0.0f && ab.time_of_impact <= dt);
	assert(ba.time_of_impact > 0.0f && ba.time_of_impact <= dt);
	assert(fabsf(ab.time_of_impact - ba.time_of_impact) < 0.001f);

	printf("PASSED\n");
	return true;
}

bool test_distance_joint_drift()
{
	printf("  test_distance_joint_drift... ");
	PhysicsScene scene;
	physics_scene_init(&scene);
	PhysicsBody sphere = {
		.orientation = quat_identity,
		.shape = { .type = SHAPE_TYPE_SPHERE, .sphere = { .radius = 0.5f } },
	};
	PhysicsConstraint joint = physics_constraint_distance_init(&scene);
	joint.body_a = physics_scene_add_body(&scene, &sphere);
	joint.anchor_a = vec3_new(0.0f, -3.0f, 0.0f);
	sphere.position.y = -3.05f;
	sphere.inverse_mass = 1.0f;
	joint.body_b = physics_scene_add_body(&scene, &sphere);
	physics_scene_add_constraint(&scene, &joint);

	const f32 dt = 1.0f / 60.0f;
	// Gravity loads the joint, but a gap inside the slop should stay at rest.
	physics_scene_update(&scene, dt);
	assert(f32_nearly_equal(joint.body_b->position.y, sphere.position.y));

	joint.body_b->position.y -= 1.0f;
	for (i32 step = 0; step < 240; ++step)
	{
		physics_scene_update(&scene, dt);
	}
	const f32 gap = joint.anchor_a.y - joint.body_b->position.y;
	assert(fabsf(gap - 0.1f) < 0.001f);
	assert(fabsf(joint.body_b->linear_velocity.y) < 0.001f);

	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_orientation_lock()
{
	printf("  test_orientation_lock... ");
	PhysicsScene scene;
	physics_scene_init(&scene);
	const PhysicsBody sphere = {
		.shape = {.type = SHAPE_TYPE_SPHERE, .sphere = {.radius = 1.0f}},
		.orientation = quat_new(vec3_new(1, 2, 0), 0.8f),
		.inverse_mass = 1.0f,
	};
	PhysicsBody* a = physics_scene_add_body(&scene, &sphere);
	PhysicsBody other = sphere;
	other.position.x = 10.0f;
	other.linear_velocity.x = 2.0f;
	other.orientation = quat_new(vec3_new(0, 1, 2), -1.3f);
	other.inverse_mass = 0.5f;
	PhysicsBody* b = physics_scene_add_body(&scene, &other);
	PhysicsConstraint joint = physics_constraint_orientation_init(a, b);
	physics_scene_add_constraint(&scene, &joint);
	const Quat rest = joint.orientation.initial_relative;
	const f32 dt = 1.0f / 60.0f;
	const Vec3 spin = vec3_new(3, -6, 1.5f);
	for (i32 run = 0; run < 2; ++run)
	{
		a->angular_velocity = spin;
		if (run == 1) { b->orientation = quat_scale(b->orientation, -1); }
		for (i32 step = 0; step < 120; ++step)
		{
			physics_scene_update(&scene, dt);
			const Quat relative = quat_mul(quat_inverse(a->orientation), b->orientation);
			assert(fabsf(quat_dot(relative, rest)) > 0.999999f);
			// B has twice A's inertia. Both should receive the momentum-weighted spin.
			assert(vec3_length(vec3_sub(b->angular_velocity, vec3_scale(spin, 1.0f / 3.0f))) < 0.0001f);
			assert(vec3_length(vec3_sub(a->angular_velocity, b->angular_velocity)) < 0.0001f);
		}
		assert(fabsf(b->position.x - a->position.x - 14.0f) < 0.001f);
		physics_scene_reset(&scene);
	}

	// Restore even a half-turn error against a fixed body at alternating timesteps.
	a->inverse_mass = 0;
	b->shape = (Shape){.type = SHAPE_TYPE_BOX, .box = box_shape_create(vec3_new(1, 2, 3))};
	b->orientation = quat_mul(quat_new(vec3_new(1, -2, 3), 3.14159265f), other.orientation);
	for (i32 step = 0; step < 180; ++step)
	{
		physics_scene_update(&scene, step % 2 ? 1.5f * dt : 0.5f * dt);
	}
	assert(quat_nearly_equal(a->orientation, sphere.orientation));
	assert(fabsf(quat_dot(b->orientation, other.orientation)) > 0.999999f);
	assert(vec3_length(b->angular_velocity) < 0.0001f);
	physics_scene_remove_body(&scene, a);
	assert(sb_count(scene.constraints) == 0);
	physics_scene_update(&scene, dt);
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

// Both fixtures have their support surface at y = 0 and touch at center height 0.5.
static PhysicsBody* test_contact_scene(PhysicsScene* scene, ShapeType shape, f32 height)
{
	physics_scene_init(scene);
	PhysicsBody support = {
		.orientation = quat_identity,
		.friction = 1.0f,
		.elasticity = 1.0f,
	};
	PhysicsBody body = support;
	if (shape == SHAPE_TYPE_SPHERE)
	{
		body.shape = support.shape = (Shape){ .type = shape, .sphere = { .radius = 0.5f } };
		support.position.y = -0.5f;
	}
	else
	{
		body.shape = (Shape){ .type = shape, .box = box_shape_create(vec3_new(0.5f, 0.5f, 0.5f)) };
		support.shape = (Shape){ .type = shape, .box = box_shape_create(vec3_new(20, 1, 20)) };
		support.position.y = -1.0f;
	}
	physics_scene_add_body(scene, &support);
	body.position.y = height;
	body.inverse_mass = 1.0f;
	return physics_scene_add_body(scene, &body);
}

bool test_resting_contacts()
{
	printf("  test_resting_contacts... ");
	const f32 dt = 1.0f / 60.0f;
	const ShapeType shapes[] = { SHAPE_TYPE_BOX, SHAPE_TYPE_SPHERE };
	for (i32 i = 0; i < ARRAY_COUNT(shapes); ++i)
	{
		PhysicsScene scene;
		PhysicsBody* body = test_contact_scene(&scene, shapes[i], 0.46f);
		PhysicsBody* support = scene.bodies[0];
		for (i32 reversed = 0; reversed < 2; ++reversed)
		{
			PhysicsContact contact = {};
			assert(physics_bodies_intersect_dt(reversed ? body : support, reversed ? support : body, dt, &contact));
			assert(contact.time_of_impact == 0.0f);
			assert(fabsf(contact.separation_distance + 0.04f) < 0.0001f);
			assert(fabsf(contact.normal.y - (reversed ? 1.0f : -1.0f)) < 0.0001f);
			PhysicsConstraintPenetration joint = physics_penetration_pre_solve(&contact, dt);
			physics_penetration_solve(&joint);
			assert(fabsf(body->linear_velocity.y - 0.3f) < 0.0001f);
			body->linear_velocity = vec3_zero;
			body->angular_velocity = vec3_zero;
		}
		for (i32 step = 0; step < 360; ++step)
		{
			physics_scene_update(&scene, dt);
			if (step >= 120)
			{
				assert(fabsf(physics_body_get_bounds(body).min.y + 0.02f) < 0.001f);
				assert(fabsf(body->linear_velocity.y) < 0.001f);
			}
		}
		physics_scene_destroy(&scene);
	}
	printf("PASSED\n");
	return true;
}

bool test_manifold_points()
{
	printf("  test_manifold_points... ");
	PhysicsScene scene;
	PhysicsBody* box = test_contact_scene(&scene, SHAPE_TYPE_BOX, 0.5f);
	PhysicsContact contact = {
		.body_a = scene.bodies[0], .body_b = box, .normal = vec3_new(0, -1, 0),
		.point_on_a_local = vec3_new(0, 1, 0), .point_on_b_local = vec3_new(0, -0.5f, 0),
	};
	physics_scene_cache_contact(&scene, &contact);
	PhysicsContactManifold* manifold = &scene.manifolds[0];
	manifold->points[0].constraint.accumulated_impulse.x = 7.0f;
	PhysicsContact reversed = {
		.body_a = box, .body_b = scene.bodies[0], .normal = vec3_negate(contact.normal),
		.point_on_a_local = contact.point_on_b_local, .point_on_b_local = contact.point_on_a_local,
	};
	reversed.point_on_a_local.x += 0.01f;
	reversed.point_on_b_local.x += 0.01f;
	physics_scene_cache_contact(&scene, &reversed);
	assert(sb_count(scene.manifolds) == 1 && manifold->count == 1);
	assert(manifold->points[0].constraint.accumulated_impulse.x == 7.0f);

	// Adding four corners should evict the center and discard its cached impulse.
	const Vec3 corners[] = {{-.5f, 0, -.5f}, {-.5f, 0, .5f}, {.5f, 0, -.5f}, {.5f, 0, .5f}};
	for (i32 i = 0; i < ARRAY_COUNT(corners); ++i)
	{
		reversed.point_on_a_local = vec3_add(contact.point_on_b_local, corners[i]);
		reversed.point_on_b_local = vec3_add(contact.point_on_a_local, corners[i]);
		physics_scene_cache_contact(&scene, &reversed);
	}
	physics_scene_cache_contact(&scene, &contact); // A fifth point at the center loses too.
	assert(manifold->count == 4);
	for (i32 i = 0; i < manifold->count; ++i)
	{
		const PhysicsManifoldPoint* point = &manifold->points[i];
		assert(fabsf(point->local_a.x) == .5f && fabsf(point->local_a.z) == .5f);
		assert(point->local_a.y == 1.0f && point->local_b.y == -.5f);
		assert(point->local_normal_a.y == 1.0f && point->constraint.accumulated_impulse.x == 0.0f);
	}

	// Slide beyond the retention distance while the two faces still touch.
	box->position.x += 0.03f;
	contact.point_on_a_local.x += 0.03f;
	physics_scene_cache_contact(&scene, &contact);
	assert(manifold->count == 1 && vec3_nearly_equal(manifold->points[0].local_a, contact.point_on_a_local));
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_manifold_warm_start()
{
	printf("  test_manifold_warm_start... ");
	PhysicsScene scene;
	PhysicsBody* box = test_contact_scene(&scene, SHAPE_TYPE_BOX, 0.5f);
	box->friction = 0.5f;
	PhysicsContact contact = {
		.body_a = scene.bodies[0], .body_b = box, .normal = vec3_new(0, -1, 0),
		.point_on_a_local = vec3_new(0, 1, 0), .point_on_b_local = vec3_new(0, -.5f, 0),
	};
	physics_scene_cache_contact(&scene, &contact);
	PhysicsConstraintPenetration* joint = &scene.manifolds[0].points[0].constraint;
	// The previous basis maps its impulse to (3, 2, 4) in world space.
	joint->basis.columns[0] = vec3_new(1, 0, 0);
	joint->basis.columns[1] = vec3_new(0, 1, 0);
	joint->basis.columns[2] = vec3_new(0, 0, 1);
	joint->accumulated_impulse = vec3_new(3, 2, 4);
	physics_manifold_pre_solve(&scene.manifolds[0], 1.0f / 30.0f, 2.0f);
	// With twice the dt, normal support becomes 4 and combined friction is capped at 2.
	const Vec3 expected = vec3_new(1.2f, 4.0f, 1.6f);
	assert(vec3_nearly_equal(mat3_mul_vec3(joint->basis, joint->accumulated_impulse), expected));
	assert(vec3_nearly_equal(box->linear_velocity, expected));
	assert(vec3_nearly_equal(scene.bodies[0]->linear_velocity, vec3_zero));
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_manifold_lifetime()
{
	printf("  test_manifold_lifetime... ");
	PhysicsScene scene;
	PhysicsBody* box = test_contact_scene(&scene, SHAPE_TYPE_BOX, 0.49f);
	PhysicsBody neighbor = *box;
	neighbor.position.x = 3.0f;
	physics_scene_add_body(&scene, &neighbor);
	Vec3 first_position = vec3_zero;
	const f32 dt = 1.0f / 60.0f;
	for (i32 run = 0; run < 2; ++run)
	{
		for (i32 i = 0; i < 30; ++i) { physics_scene_update(&scene, i % 2 ? 1.5f * dt : 0.5f * dt); }
		assert(sb_count(scene.manifolds) == 2);
		assert(scene.manifolds[0].points[0].constraint.accumulated_impulse.x > 0.0f);
		if (run == 0)
		{
			first_position = box->position;
			physics_scene_reset(&scene);
			assert(sb_count(scene.manifolds) == 0 && scene.previous_dt == 0.0f);
		}
		else { assert(vec3_nearly_equal(box->position, first_position)); }
	}
	physics_scene_remove_body(&scene, scene.bodies[2]);
	assert(sb_count(scene.manifolds) == 1);
	box->linear_velocity.y = 3.0f;
	for (i32 i = 1; i <= 2; ++i)
	{
		physics_scene_update(&scene, dt);
		assert(fabsf(box->linear_velocity.y - (3.0f - i * 10.0f * dt)) < 0.0001f);
	}
	assert(sb_count(scene.manifolds) == 0);
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_joint_supported_by_contact()
{
	printf("  test_joint_supported_by_contact... ");
	PhysicsScene scene;
	PhysicsBody* base = test_contact_scene(&scene, SHAPE_TYPE_BOX, 0.49f);
	PhysicsBody load = *base;
	load.position.y = 2.5f;
	PhysicsConstraint joint = physics_constraint_distance_init(&scene);
	joint.body_a = base;
	joint.anchor_a = vec3_new(0, 2, 0);
	joint.body_b = physics_scene_add_body(&scene, &load);
	physics_scene_add_constraint(&scene, &joint);

	// Contact support must reach the load through the joint within this same step.
	physics_scene_update(&scene, 1.0f / 60.0f);
	assert(fabsf(base->linear_velocity.y) < 0.001f);
	assert(fabsf(joint.body_b->linear_velocity.y) < 0.02f);
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_contact_friction()
{
	printf("  test_contact_friction... ");
	const f32 dt = 1.0f / 60.0f;
	const f32 coefficients[] = { 0.0f, 0.8f };
	for (i32 i = 0; i < ARRAY_COUNT(coefficients); ++i)
	{
		PhysicsScene scene;
		PhysicsBody* box = test_contact_scene(&scene, SHAPE_TYPE_BOX, 0.5f);
		box->friction = coefficients[i];
		box->elasticity = 0.0f;
		box->linear_velocity = vec3_new(1.2f, 0.0f, 1.6f);
		physics_scene_update(&scene, dt);
		// Sliding diagonally exercises the combined bound on both tangent impulses.
		f32 speed = hypotf(box->linear_velocity.x, box->linear_velocity.z);
		assert(fabsf(speed - (2.0f - box->friction * 10.0f * dt)) < 0.001f);
		for (i32 step = 1; step < 60; ++step)
		{
			physics_scene_update(&scene, dt);
		}
		speed = hypotf(box->linear_velocity.x, box->linear_velocity.z);
		if (box->friction > 0.0f) { assert(speed < 0.3f); }
		else { assert(fabsf(speed - 2.0f) < 0.01f); }
		physics_scene_destroy(&scene);
	}
	printf("PASSED\n");
	return true;
}

bool test_contact_lift_off_and_bounce()
{
	printf("  test_contact_lift_off_and_bounce... ");
	const f32 dt = 1.0f / 60.0f;
	PhysicsScene scene;
	PhysicsBody* box = test_contact_scene(&scene, SHAPE_TYPE_BOX, 0.49f);
	box->linear_velocity = vec3_new(1.0f, 2.0f, 0.0f);
	physics_scene_update(&scene, dt);
	assert(fabsf(box->linear_velocity.y - (2.0f - 10.0f * dt)) < 0.0001f);
	assert(fabsf(box->linear_velocity.x - 1.0f) < 0.0001f);
	assert(box->position.y > 0.5f);

	box->position = vec3_new(0.0f, 0.6f, 0.0f);
	box->linear_velocity = vec3_new(0.0f, -12.0f, 0.0f);
	box->elasticity = 0.8f;
	physics_scene_update(&scene, dt);
	assert(fabsf(box->linear_velocity.y - 0.8f * (12.0f + 10.0f * dt)) < 0.001f);
	assert(box->position.y > 0.5f);
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_kinematic_collider()
{
	printf("  test_kinematic_collider... ");
	PhysicsScene scene;
	physics_scene_init(&scene);
	const Shape box = {
		.type = SHAPE_TYPE_BOX,
		.box = box_shape_create(vec3_new(1, 1, 1)),
	};
	PhysicsBody* player = physics_scene_add_body(&scene, &(PhysicsBody) {
		.shape = box, .orientation = quat_identity, .inverse_mass = 0.0f,
		.position = vec3_new(-4, 0, 0), .linear_velocity = vec3_new(500, 0, 0),
	});
	PhysicsBody* crate = physics_scene_add_body(&scene, &(PhysicsBody) {
		.shape = box, .orientation = quat_identity, .inverse_mass = 1.0f,
	});
	// At sprint speed the player would cross the entire crate in one frame.
	const f32 dt = 1.0f / 60.0f;
	physics_scene_update(&scene, dt);
	assert(fabsf(player->position.x - (-4.0f + 500.0f * dt)) < 0.0001f);
	assert(player->position.y == 0 && player->linear_velocity.x == 500);
	assert(vec3_nearly_equal(player->angular_velocity, vec3_zero));
	assert(crate->position.x > player->position.x && crate->linear_velocity.x > 0);

	// Stop input and send the crate back into the stationary player.
	const Vec3 stopped = player->position;
	player->linear_velocity = vec3_zero;
	crate->position = vec3_add(stopped, vec3_new(2, 0, 0));
	crate->orientation = quat_identity;
	crate->angular_velocity = vec3_zero;
	crate->linear_velocity = vec3_new(-10, 0, 0);
	physics_scene_update(&scene, dt);
	assert(vec3_nearly_equal(player->position, stopped));
	assert(vec3_nearly_equal(player->linear_velocity, vec3_zero));
	assert(vec3_nearly_equal(player->angular_velocity, vec3_zero));
	assert(crate->linear_velocity.x > -0.01f);
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_physics_scene_reset()
{
	printf("  test_physics_scene_reset... ");
	PhysicsScene scene;
	physics_scene_init(&scene);
	const PhysicsBody initial = {
		.position = vec3_new(0, 3, 0),
		.orientation = quat_new(vec3_new(0, 1, 0), 0.5f),
		.linear_velocity = vec3_new(1, 0, 0),
		.angular_velocity = vec3_new(0, 2, 0),
		.inverse_mass = 1.0f,
		.shape = { .type = SHAPE_TYPE_SPHERE, .sphere = { .radius = 0.5f } },
	};
	PhysicsBody* body = physics_scene_add_body(&scene, &initial);
	PhysicsBody base = initial;
	base.position = vec3_zero;
	base.inverse_mass = 0.0f;
	base.angular_velocity = vec3_zero;
	PhysicsBody* support = physics_scene_add_body(&scene, &base);
	PhysicsConstraint joint = physics_constraint_distance_init(&scene);
	joint.body_a = support;
	joint.body_b = body;
	joint.anchor_a = vec3_new(0, 2, 0);
	physics_scene_add_constraint(&scene, &joint);

	Vec3 first_run_position = vec3_zero;
	for (i32 run = 0; run < 2; ++run)
	{
		for (i32 step = 0; step < 12; ++step) { physics_scene_update(&scene, 1.0f / 60.0f); }
		if (run == 0) { first_run_position = body->position; }
		else { assert(vec3_nearly_equal(first_run_position, body->position)); }
		assert(!vec3_nearly_equal(body->position, initial.position));
		assert(scene.constraints[0].distance.cached_lambda.data[0] != 0.0f);

		physics_scene_reset(&scene);
		assert(vec3_nearly_equal(body->position, initial.position));
		assert(quat_nearly_equal(body->orientation, initial.orientation));
		assert(vec3_nearly_equal(body->linear_velocity, initial.linear_velocity));
		assert(vec3_nearly_equal(body->angular_velocity, initial.angular_velocity));
		assert(vec3_nearly_equal(support->position, base.position));
		assert(scene.constraints[0].distance.cached_lambda.data[0] == 0.0f);
		assert(scene.constraints[0].distance.baumgarte_bias == 0.0f);
	}
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_physics_scene_remove_body()
{
	printf("  test_physics_scene_remove_body... ");
	PhysicsScene scene;
	physics_scene_init(&scene);
	PhysicsBody sphere = {
		.orientation = quat_identity,
		.inverse_mass = 1.0f,
		.shape = { .type = SHAPE_TYPE_SPHERE, .sphere = { .radius = 0.5f } },
	};
	PhysicsBody* links[3];
	for (i32 i = 0; i < ARRAY_COUNT(links); ++i)
	{
		sphere.position.y = 2.0f * i;
		links[i] = physics_scene_add_body(&scene, &sphere);
	}
	for (i32 i = 0; i < ARRAY_COUNT(links); ++i)
	{
		PhysicsConstraint joint = physics_constraint_distance_init(&scene);
		joint.body_a = links[i];
		joint.body_b = links[(i + 1) % ARRAY_COUNT(links)];
		physics_scene_add_constraint(&scene, &joint);
	}
	physics_scene_remove_body(&scene, links[1]);
	assert(sb_count(scene.bodies) == 2 && sb_count(scene.constraints) == 1);
	assert(scene.constraints[0].body_a == links[2] && scene.constraints[0].body_b == links[0]);
	physics_scene_update(&scene, 1.0f / 60.0f);
	physics_scene_reset(&scene);
	assert(links[0]->position.y == 0.0f && links[2]->position.y == 4.0f);
	physics_scene_destroy(&scene);
	printf("PASSED\n");
	return true;
}

bool test_lcp_op_begin_end()
{
	printf("  test_lcp_op_begin_end... ");

	Arena* arena = arena_create(&default_arena_desc);

	VecN a = vecn_new(arena, 3);
	a.data[0] = 1.0f; a.data[1] = 2.0f; a.data[2] = 3.0f;

	VecN b = vecn_scale(arena, &a, 2.0f);
	assert(f32_nearly_equal(b.data[0], 2.0f));
	assert(f32_nearly_equal(b.data[1], 4.0f));
	assert(f32_nearly_equal(b.data[2], 6.0f));

	VecN c = vecn_copy(arena, &a);
	assert(f32_nearly_equal(c.data[0], 1.0f));

	VecN d = vecn_add(arena, &a, &b);
	assert(f32_nearly_equal(d.data[0], 3.0f));
	assert(f32_nearly_equal(d.data[1], 6.0f));
	assert(f32_nearly_equal(d.data[2], 9.0f));

	MatMN m1 = matmn_new(arena, 2, 3);
	m1.rows[0].data[0] = 1.0f; m1.rows[0].data[1] = 0.0f; m1.rows[0].data[2] = 0.0f;
	m1.rows[1].data[0] = 0.0f; m1.rows[1].data[1] = 1.0f; m1.rows[1].data[2] = 0.0f;

	MatMN m2 = matmn_new(arena, 3, 2);
	m2.rows[0].data[0] = 1.0f; m2.rows[0].data[1] = 2.0f;
	m2.rows[1].data[0] = 3.0f; m2.rows[1].data[1] = 4.0f;
	m2.rows[2].data[0] = 5.0f; m2.rows[2].data[1] = 6.0f;

	MatMN m3 = matmn_mul_matmn(arena, &m1, &m2);
	assert(f32_nearly_equal(m3.rows[0].data[0], 1.0f));
	assert(f32_nearly_equal(m3.rows[0].data[1], 2.0f));
	assert(f32_nearly_equal(m3.rows[1].data[0], 3.0f));
	assert(f32_nearly_equal(m3.rows[1].data[1], 4.0f));

	arena_destroy(&arena);

	printf("PASSED\n");
	return true;
}

// Exact signed area of a glyph's closed quadratic contours via Green's theorem
static f64 ttf_glyph_exact_area(const TTFFont* font, u32 codepoint)
{
	const TTFGlyphOutline* outline = &font->glyph_outlines[codepoint];
	f64 area = 0.0;
	for (u32 i = 0; i < outline->curve_count; ++i)
	{
		const TTFCurve* c = &font->curves[outline->curve_offset + i];
		const f64 p1_p2 = (f64)c->p1.x * c->p2.y - (f64)c->p1.y * c->p2.x;
		const f64 p2_p3 = (f64)c->p2.x * c->p3.y - (f64)c->p2.y * c->p3.x;
		const f64 p1_p3 = (f64)c->p1.x * c->p3.y - (f64)c->p1.y * c->p3.x;
		area += (2.0 * p1_p2 + 2.0 * p2_p3 + p1_p3) / 6.0;
	}
	return area;
}

// Integrates one ray's signed coverage: one row per font unit (TrueType's horizontal edges lie on integers, so rows
// sit between them), samples one pixel apart along the ray. Each row's sum telescopes to its exact chord length.
// Sample offsets aren't dyadic so translating control points rounds, as it does for real pixel positions.
static f64 ttf_slug_ray_area(const TTFFont* font, u32 codepoint, bool swap_axes)
{
	const f32 units_per_pixel = 32.0f;
	const f32 row_offset = 0.5f + 1.0f / 192.0f;
	const TTFGlyphOutline* o = &font->glyph_outlines[codepoint];
	const i32 row_min = swap_axes ? o->x_min : o->y_min;
	const i32 row_max = swap_axes ? o->x_max : o->y_max;
	const f32 along_min = (swap_axes ? o->y_min : o->x_min) - 2.0f * units_per_pixel + units_per_pixel / 3.0f;
	const i32 num_samples = (i32)(((swap_axes ? o->y_max : o->x_max) - along_min) / units_per_pixel) + 3;

	f64 area = 0.0;
	for (i32 row = row_min - 1; row <= row_max; ++row)
	{
		for (i32 s = 0; s < num_samples; ++s)
		{
			const Vec2 origin = vec2_new(along_min + s * units_per_pixel, row + row_offset);
			for (u32 i = 0; i < o->curve_count; ++i)
			{
				const TTFCurve* c = &font->curves[o->curve_offset + i];
				const Vec2 p1 = swap_axes ? vec2_new(c->p1.y, c->p1.x) : c->p1;
				const Vec2 p2 = swap_axes ? vec2_new(c->p2.y, c->p2.x) : c->p2;
				const Vec2 p3 = swap_axes ? vec2_new(c->p3.y, c->p3.x) : c->p3;
				area += ttf_slug_ray_coverage(p1, p2, p3, origin, 1.0f / units_per_pixel);
			}
		}
	}
	return area * units_per_pixel;
}

bool test_ttf_slug_coverage()
{
	printf("  test_ttf_slug_coverage... ");

	TTFFont font;
	if (!ttf_init(&font, "data/fonts/Menlo-Regular.ttf"))
	{
		printf("FAILED (could not load font)\n");
		return false;
	}

	const f32 pixels_per_unit = ttf_scale_for_pixel_height(&font, 24.0f);

	// Middle of the I stem is fully covered, far outside it is empty
	const TTFGlyphOutline* i_outline = &font.glyph_outlines['I'];
	const f32 i_mid_x = (i_outline->x_min + i_outline->x_max) * 0.5f;
	const f32 i_mid_y = (i_outline->y_min + i_outline->y_max) * 0.5f;
	assert(ttf_slug_coverage(&font, 'I', i_mid_x, i_mid_y, pixels_per_unit) > 0.99f);
	assert(ttf_slug_coverage(&font, 'I', i_outline->x_max + 1000.0f, i_mid_y, pixels_per_unit) == 0.0f);

	// Middle of the o's hole is empty: the inner contour cancels the outer one
	const TTFGlyphOutline* o_outline = &font.glyph_outlines['o'];
	const f32 o_mid_x = (o_outline->x_min + o_outline->x_max) * 0.5f;
	const f32 o_mid_y = (o_outline->y_min + o_outline->y_max) * 0.5f;
	assert(ttf_slug_coverage(&font, 'o', o_mid_x, o_mid_y, pixels_per_unit) < 0.01f);

	// Every printable glyph: both rays integrate to the exact area
	f64 worst_error = 0.0;
	for (u32 c = 33; c < 127; ++c)
	{
		const f64 exact = fabs(ttf_glyph_exact_area(&font, c));
		assert(exact > 0.0);
		for (i32 swap_axes = 0; swap_axes <= 1; ++swap_axes)
		{
			const f64 error = fabs(fabs(ttf_slug_ray_area(&font, c, swap_axes)) - exact) / exact;
			worst_error = error > worst_error ? error : worst_error;
			assert(error < 1e-3);
		}
	}

	ttf_free(&font);

	printf("PASSED (worst area error %.5f%%)\n", worst_error * 100.0);
	return true;
}

// Reference coverage without bands or early exit: every curve, both rays
static f32 ttf_slug_coverage_all_curves(const TTFFont* font, u32 codepoint, f32 x, f32 y, f32 pixels_per_unit)
{
	const TTFGlyphOutline* outline = &font->glyph_outlines[codepoint];
	f32 x_coverage = 0.0f;
	f32 y_coverage = 0.0f;
	for (u32 i = 0; i < outline->curve_count; ++i)
	{
		const TTFCurve* c = &font->curves[outline->curve_offset + i];
		x_coverage += ttf_slug_ray_coverage(c->p1, c->p2, c->p3, vec2_new(x, y), pixels_per_unit);
		y_coverage += ttf_slug_ray_coverage(
			vec2_new(c->p1.y, c->p1.x), vec2_new(c->p2.y, c->p2.x), vec2_new(c->p3.y, c->p3.x), vec2_new(y, x),
			pixels_per_unit);
	}
	return (ttf_saturate(fabsf(x_coverage)) + ttf_saturate(fabsf(y_coverage))) * 0.5f;
}

bool test_ttf_slug_bands()
{
	printf("  test_ttf_slug_bands... ");

	TTFFont font;
	if (!ttf_init(&font, "data/fonts/Menlo-Regular.ttf"))
	{
		printf("FAILED (could not load font)\n");
		return false;
	}

	const f32 pixel_heights[] = { 8.0f, 24.0f, 96.0f };
	const i32 grid_size = 61;
	f32 worst_difference = 0.0f;
	u32 num_samples = 0;

	for (u32 c = 33; c < 127; ++c)
	{
		const TTFGlyphOutline* o = &font.glyph_outlines[c];
		assert(o->band_count >= 1 && o->band_count <= TTF_MAX_BANDS);

		// Grid over the glyph's box plus a margin, with spacing that isn't a whole number of font units
		const f32 margin_x = (o->x_max - o->x_min) * 0.1f;
		const f32 margin_y = (o->y_max - o->y_min) * 0.1f;
		const f32 step_x = (o->x_max - o->x_min + 2.0f * margin_x) / (grid_size - 0.7f);
		const f32 step_y = (o->y_max - o->y_min + 2.0f * margin_y) / (grid_size - 0.7f);

		for (u32 h = 0; h < ARRAY_COUNT(pixel_heights); ++h)
		{
			const f32 pixels_per_unit = ttf_scale_for_pixel_height(&font, pixel_heights[h]);
			for (i32 gy = 0; gy < grid_size + (i32)o->band_count + 1; ++gy)
			{
				for (i32 gx = 0; gx < grid_size + (i32)o->band_count + 1; ++gx)
				{
					// Past the regular grid, coordinates land exactly on band boundaries
					const f32 band_x = o->x_min + (gx - grid_size) * ((f32)(o->x_max - o->x_min) / o->band_count);
					const f32 band_y = o->y_min + (gy - grid_size) * ((f32)(o->y_max - o->y_min) / o->band_count);
					const f32 x = gx < grid_size ? o->x_min - margin_x + gx * step_x : band_x;
					const f32 y = gy < grid_size ? o->y_min - margin_y + gy * step_y : band_y;

					const f32 banded = ttf_slug_coverage(&font, c, x, y, pixels_per_unit);
					const f32 reference = ttf_slug_coverage_all_curves(&font, c, x, y, pixels_per_unit);
					const f32 difference = fabsf(banded - reference);
					worst_difference = difference > worst_difference ? difference : worst_difference;
					assert(difference < 1e-5f);
					++num_samples;
				}
			}
		}
	}

	ttf_free(&font);

	printf("PASSED (%u samples, worst difference %g)\n", num_samples, worst_difference);
	return true;
}
