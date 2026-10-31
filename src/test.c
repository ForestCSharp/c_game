
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
#include <string.h>
#include "file_helpers.h"
#include "truetype.h"

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
bool test_ttf_slug_coverage();
bool test_ttf_slug_bands();

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
