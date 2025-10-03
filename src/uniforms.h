#pragma once

#include "math/basic_math.h"
#include "math/vec.h"
#include "math/matrix.h"

typedef struct GlobalUniformStruct
{
	Mat4 view;
	Mat4 projection;
	Vec4 eye;
	Vec4 light_dir;
} GlobalUniformStruct;

typedef struct ObjectUniformStruct
{
	Mat4 model;
	bool is_skinned;
	uint padding[3];
} ObjectUniformStruct;
