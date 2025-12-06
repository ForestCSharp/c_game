
#include "math.h"
#include "types.h"
#include "stdio.h"

bool test_mat4_inverse();
bool test_mat4_decompose();
bool test_quat_mat_conversions();

int main()
{
	bool success = true;
	success &= test_mat4_inverse();
	success &= test_mat4_decompose();
	success &= test_quat_mat_conversions();


	if (!success)
	{
		printf("TESTS FAILED\n");
		return 1;
	}

	printf("ALL TESTS PASSED!\n");
	return 0;
}

bool test_mat4_inverse()
{
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

	return true;
}

bool test_mat4_decompose()
{
	Vec3 scale = vec3_new(1,2,3);
	Mat4 scale_matrix = mat4_scale(scale);

	Quat rotation = quat_new(vec3_new(1, 0, 0), 0.5);
	Mat4 rotation_matrix = quat_to_mat4(rotation);

	Vec3 translation = vec3_new(4,5,6);
	Mat4 translation_matrix = mat4_translation(translation);

	Mat4 original_matrix = mat4_mul_mat4(mat4_mul_mat4(scale_matrix, rotation_matrix), translation_matrix);
	TRS trs = mat4_to_trs(original_matrix);
	Mat4 recomposed_matrix = mat4_mul_mat4(mat4_mul_mat4(mat4_scale(trs.scale), quat_to_mat4(trs.rotation)), mat4_translation(trs.translation));

	return 	vec3_nearly_equal(scale, trs.scale)
		&&	quat_nearly_equal(rotation, trs.rotation)
		&& 	vec3_nearly_equal(translation, trs.translation)
		&& 	mat4_nearly_equal(original_matrix, recomposed_matrix);
}

bool test_quat_mat_conversions()
{
	Quat q1 = quat_normalize(quat_new(vec3_new(1, 1, 0), 0.5));
	Mat4 m1 = quat_to_mat4(q1);
	Quat q2 = mat4_to_quat(m1);
	Mat4 m2 = quat_to_mat4(q2);

	return quat_nearly_equal(q1, q2) && mat4_nearly_equal(m1,m2);
}
