#include "gpu.h"

u32 gpu_format_stride(GpuFormat format)
{
    switch (format)
    {
    case GPU_FORMAT_R32_SINT:
    case GPU_FORMAT_RGBA8_UNORM:
    case GPU_FORMAT_BGRA8_UNORM:
    case GPU_FORMAT_RGBA8_SRGB:
    case GPU_FORMAT_BGRA8_SRGB:
        return 4;
    case GPU_FORMAT_RG32_SFLOAT:
        return sizeof(float) * 2;
    case GPU_FORMAT_RGB32_SFLOAT:
        return sizeof(float) * 3;
    case GPU_FORMAT_RGBA32_SFLOAT:
        return sizeof(float) * 4;
    default:
        printf("Error: Unhandled Format in gpu_format_stride\n");
        exit(0);
        return 0;
    }
}

#if defined(GPU_IMPLEMENTATION_VULKAN)
#include "vulkan/gpu_vulkan.c"
#elif defined(GPU_IMPLEMENTATION_METAL)
#include "metal/gpu_metal.c"
#endif

