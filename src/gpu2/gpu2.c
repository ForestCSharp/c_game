#include "gpu2.h"

u32 gpu2_format_stride(Gpu2Format format)
{
    switch (format)
    {
    case GPU2_FORMAT_R32_SINT:
    case GPU2_FORMAT_RGBA8_UNORM:
    case GPU2_FORMAT_BGRA8_UNORM:
    case GPU2_FORMAT_RGBA8_SRGB:
    case GPU2_FORMAT_BGRA8_SRGB:
        return 4;
    case GPU2_FORMAT_RG32_SFLOAT:
        return sizeof(float) * 2;
    case GPU2_FORMAT_RGB32_SFLOAT:
        return sizeof(float) * 3;
    case GPU2_FORMAT_RGBA32_SFLOAT:
        return sizeof(float) * 4;
    default:
        printf("Error: Unhandled Format in gpu2_format_stride\n");
        exit(0);
        return 0;
    }
}

#if defined(GPU2_IMPLEMENTATION_VULKAN)
#include "vulkan/gpu2_vulkan.c"
#elif defined(GPU2_IMPLEMENTATION_METAL)
#include "metal/gpu2_metal.c"
#endif

