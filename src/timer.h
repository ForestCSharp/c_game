#include "types.h"

#if defined(_WIN32)

// FCS TODO: QueryPerformanceCounter...
#pragma message "TODO: NOT YET IMPLEMENTED ON WINDOWS"

#elif defined(__APPLE__)

#include "mach/mach_time.h"

u64 time_now()
{
    return mach_absolute_time();
}

double time_seconds(u64 in_time)
{
    // FCS TODO: Cache Timebase info (lazy init?)
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    double nanoseconds = (in_time * info.numer) / info.denom;
    return nanoseconds / 1e9;
}

#endif
