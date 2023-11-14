#include "types.h"

#if defined(_WIN32)

//#include "profileapi.h" // FCS TODO: QueryPerformanceCounter...
#include "time.h"

//NOTE: windows clock() function returns wall time, rather than cpu time used by the process

u64 time_now()
{
    // clock() returns a 32-bit int on windows
    return (u64) clock();
}

double time_seconds(u64 in_time)
{
    return (double) in_time / (double) CLOCKS_PER_SEC;
}

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