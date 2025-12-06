#pragma once

#include "math/math_lib.h"

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
