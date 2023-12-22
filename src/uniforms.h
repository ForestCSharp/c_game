#pragma once

#include "math/basic_math.h"
#include "math/vec.h"
#include "math/matrix.h"

typedef struct GlobalUniformStruct
{
	Mat4 model;
	Mat4 view;
	Mat4 projection;
	Mat4 mvp;
	Vec4 eye;
	Vec4 light_dir;
	bool is_colliding;
} GlobalUniformStruct;

typedef struct ObjectUniformStruct
{
	Mat4 model;
	u32 vertex_buffer_idx;
	u32 index_buffer_idx;
} ObjectUniformStruct;
