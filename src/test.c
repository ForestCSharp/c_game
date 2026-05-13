
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

bool test_mat3_inverse();
bool test_mat4_inverse();
bool test_mat4_decompose();
bool test_quat_mat_conversions();
bool test_stretchy_buffer();
bool test_matn_mul_matn();
bool test_matmn_mul_matmn();
bool test_matn_from_matmn();
bool test_lcp_op_begin_end();
bool test_arena_create_destroy();
bool test_arena_alloc();
bool test_arena_multiple_allocs();
bool test_arena_exact_fill();
bool test_arena_oom_detection();
bool test_arena_oom_null_return();
bool test_arena_growth();

int main()
{
	bool success = true;
	success &= test_mat3_inverse();
	success &= test_mat4_inverse();
	success &= test_mat4_decompose();
	success &= test_quat_mat_conversions();
	success &= test_stretchy_buffer();
	success &= test_matn_mul_matn();
	success &= test_matmn_mul_matmn();
	success &= test_matn_from_matmn();
	success &= test_lcp_op_begin_end();
	success &= test_arena_create_destroy();
	success &= test_arena_alloc();
	success &= test_arena_multiple_allocs();
	success &= test_arena_exact_fill();
	success &= test_arena_oom_detection();
	success &= test_arena_oom_null_return();
	success &= test_arena_growth();


	if (!success)
	{
		printf("TESTS FAILED\n");
		return 1;
	}

	printf("ALL TESTS PASSED!\n");
	return 0;
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
    MatN A = matn_new(arena, 2);
    A.rows[0].data[0] = 1.0f; A.rows[0].data[1] = 2.0f;
    A.rows[1].data[0] = 3.0f; A.rows[1].data[1] = 4.0f;

    MatN B = matn_new(arena, 2);
    B.rows[0].data[0] = 5.0f; B.rows[0].data[1] = 6.0f;
    B.rows[1].data[0] = 7.0f; B.rows[1].data[1] = 8.0f;

    MatN C = matn_mul_matn(arena, &A, &B);
    assert(f32_nearly_equal(C.rows[0].data[0], 19.0f));
    assert(f32_nearly_equal(C.rows[0].data[1], 22.0f));
    assert(f32_nearly_equal(C.rows[1].data[0], 43.0f));
    assert(f32_nearly_equal(C.rows[1].data[1], 50.0f));

	arena_destroy(arena);

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

	arena_destroy(arena);

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

	arena_destroy(arena);

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

	arena_destroy(arena);

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

	arena_destroy(arena);

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

	arena_destroy(arena);

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

	arena_destroy(arena);

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

	arena_destroy(arena);

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
	MatMN A = matmn_new(arena, 2, 3);
	A.rows[0].data[0] = 1.0f; A.rows[0].data[1] = 2.0f; A.rows[0].data[2] = 3.0f;
	A.rows[1].data[0] = 4.0f; A.rows[1].data[1] = 5.0f; A.rows[1].data[2] = 6.0f;

	MatMN B = matmn_new(arena, 3, 2);
	B.rows[0].data[0] = 7.0f;  B.rows[0].data[1] = 8.0f;
	B.rows[1].data[0] = 9.0f;  B.rows[1].data[1] = 10.0f;
	B.rows[2].data[0] = 11.0f; B.rows[2].data[1] = 12.0f;

	MatMN C = matmn_mul_matmn(arena, &A, &B);
	assert(C.m == 2 && C.n == 2);
	assert(f32_nearly_equal(C.rows[0].data[0], 58.0f));
	assert(f32_nearly_equal(C.rows[0].data[1], 64.0f));
	assert(f32_nearly_equal(C.rows[1].data[0], 139.0f));
	assert(f32_nearly_equal(C.rows[1].data[1], 154.0f));

	arena_destroy(arena);

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

	arena_destroy(arena);

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

	MatMN M1 = matmn_new(arena, 2, 3);
	M1.rows[0].data[0] = 1.0f; M1.rows[0].data[1] = 0.0f; M1.rows[0].data[2] = 0.0f;
	M1.rows[1].data[0] = 0.0f; M1.rows[1].data[1] = 1.0f; M1.rows[1].data[2] = 0.0f;

	MatMN M2 = matmn_new(arena, 3, 2);
	M2.rows[0].data[0] = 1.0f; M2.rows[0].data[1] = 2.0f;
	M2.rows[1].data[0] = 3.0f; M2.rows[1].data[1] = 4.0f;
	M2.rows[2].data[0] = 5.0f; M2.rows[2].data[1] = 6.0f;

	MatMN M3 = matmn_mul_matmn(arena, &M1, &M2);
	assert(f32_nearly_equal(M3.rows[0].data[0], 1.0f));
	assert(f32_nearly_equal(M3.rows[0].data[1], 2.0f));
	assert(f32_nearly_equal(M3.rows[1].data[0], 3.0f));
	assert(f32_nearly_equal(M3.rows[1].data[1], 4.0f));

	arena_destroy(arena);

	printf("PASSED\n");
	return true;
}
