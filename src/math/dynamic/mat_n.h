#pragma once

#include "math/basic_math.h"
#include "math/dynamic/vec_n.h"
#include "stretchy_buffer.h"

typedef struct MatN
{
	sbuffer(VecN) rows;	
} MatN;

//FCS TODO:
