#ifndef STB_STRETCHY_BUFFER_H_INCLUDED
#define STB_STRETCHY_BUFFER_H_INCLUDED

#ifndef NO_STRETCHY_BUFFER_SHORT_NAMES

#define sb_free  stb_sb_free
#define sb_push  stb_sb_push
#define sb_count stb_sb_count
#define sb_add   stb_sb_add
#define sb_last  stb_sb_last

#define sb_del  stb_sb_del
#define sb_deln stb_sb_deln
#define sb_ins  stb_sb_ins
#define sb_insn stb_sb_insn

#endif

#define stb_sb_free(a) \
    ((a) ? free(stb__sbraw(a)), a = NULL, 0 : 0) // FCS Modification: set input 'a' to NULL after free
#define stb_sb_push(a, v) (stb__sbmaybegrow(a, 1), (a)[stb__sbn(a)++] = (v))
#define stb_sb_count(a)   ((a) ? stb__sbn(a) : 0)
#define stb_sb_add(a, n)  (stb__sbmaybegrow(a, n), stb__sbn(a) += (n), &(a)[stb__sbn(a) - (n)])
#define stb_sb_last(a)    ((a)[stb__sbn(a) - 1])

// BEGIN FCS: adapted from stbds
#define stb_sb_deln(a, i, n) (memmove(&(a)[i], &(a)[(i) + (n)], sizeof *(a) * (stb__sbn(a) - (n) - (i))), stb__sbn(a) -= (n))
#define stb_sb_del(a, i)     stb_sb_deln(a, i, 1)

#define stb_sb_insn(a, i, n) (stb_sb_add((a), (n)), memmove(&(a)[(i) + (n)], &(a)[i], sizeof *(a) * (stb__sbn(a) - (n) - (i))))
#define stb_sb_ins(a, i, v)  (stb_sb_insn((a), (i), 1), (a)[i] = (v))
// END FCS: adapted from stbds

#define stb__sbraw(a) ((int*) (void*) (a) -2) // actual start of malloc'd data (the two integers described below)
#define stb__sbm(a)   stb__sbraw(a)[0] // array capacity
#define stb__sbn(a)   stb__sbraw(a)[1] // array count

#define stb__sbneedgrow(a, n)  ((a) == 0 || stb__sbn(a) + (n) >= stb__sbm(a))
#define stb__sbmaybegrow(a, n) (stb__sbneedgrow(a, (n)) ? stb__sbgrow(a, n) : 0)
#define stb__sbgrow(a, n)      (*((void**) &(a)) = stb__sbgrowf((a), (n), sizeof(*(a))))

#include <stdlib.h>

static void* stb__sbgrowf(void* arr, int increment, int itemsize)
{
    int dbl_cur = arr ? 2 * stb__sbm(arr) : 0;
    int min_needed = stb_sb_count(arr) + increment;
    int m = dbl_cur > min_needed ? dbl_cur : min_needed;
    int* p = (int*) realloc(arr ? stb__sbraw(arr) : 0, itemsize * m + sizeof(int) * 2);
    if (p)
    {
        if (!arr)
        {
            p[1] = 0;
        }
        p[0] = m;
        return p + 2;
    }
    else
    {
#ifdef STRETCHY_BUFFER_OUT_OF_MEMORY
        STRETCHY_BUFFER_OUT_OF_MEMORY;
#endif
        return (void*) (2 * sizeof(int)); // try to force a NULL pointer exception later
    }
}
#endif // STB_STRETCHY_BUFFER_H_INCLUDED

// useful for denoting that a type is a stretchy buffer and not just a regular pointer/array
#define sbuffer(t) t*
