
#include "matrix.h"
#include "types.h"
#include "stdio.h"

bool test_mat4();
bool test_quat();

int main()
{
	bool success = true;
	success &= test_mat4();
	success &= test_quat();


	if (!success)
	{
		printf("TESTS FAILED\n");
		return 1;
	}

	printf("ALL TESTS PASSED!\n");
	return 0;
}

bool test_mat4()
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
	  			assert(float_nearly_equal(result.d[row][col], mat4_identity.d[row][col]));
	  		}
	}

	return true;
}

bool test_quat()
{
	return true;
}
