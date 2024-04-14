#include "gpu2.h"

#if defined(GPU2_IMPLEMENTATION_VULKAN)
#include "vulkan/gpu2_vulkan.c"
#elif defined(GPU2_IMPLEMENTATION_METAL)
#include "metal/gpu2_metal.c"
#endif

