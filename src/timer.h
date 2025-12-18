#include "basic_types.h"

#if defined(_WIN32)

#include <windows.h>

// Global or static variable to cache the frequency
static double g_performance_frequency = 0.0;

u64 time_now()
{
    LARGE_INTEGER counter;
    if (QueryPerformanceCounter(&counter))
    {
        return (u64)counter.QuadPart;
    }
    return 0;
}

double time_seconds(u64 in_time)
{
    // Initialize frequency if it hasn't been set yet
    if (g_performance_frequency == 0.0)
    {
        LARGE_INTEGER freq;
        if (QueryPerformanceFrequency(&freq))
        {
            g_performance_frequency = (double)freq.QuadPart;
        }
        else
        {
            // This should never happen on modern Windows (XP or later)
            return 0.0;
        }
    }

    // Calculation: Ticks / Ticks-per-Second
    return (double)in_time / g_performance_frequency;
}

#elif defined(__APPLE__)

#include "mach/mach_time.h"

u64 time_now()
{
    return mach_absolute_time();
}

double _mac_time_nanoseconds(u64 in_time)
{
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    double nanoseconds = (in_time * info.numer) / info.denom;
    return nanoseconds;
}

double time_seconds(u64 in_time)
{
	return _mac_time_nanoseconds(in_time) / 1e9;
}

#endif
