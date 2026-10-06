/*
 * CRASH-14: 5.3.0 swmm_getCount / swmm_getIndex index past Nobjects[] and
 * Htable[] for object types 18..99.
 *
 * 5.3.0 widened the accepted object types from swmm_GAGE..swmm_LINK (5.2.4)
 * to everything below swmm_SYSTEM (100), but the internal arrays Nobjects[]
 * and Htable[] have MAX_OBJ_TYPES = 18 entries (swmm_GAGE = 0 .. swmm_INLET
 * = 17). swmm_getCount(50) returns Nobjects[50], a read past the array, and
 * swmm_getIndex(50, "J1") hands Htable[50], a garbage pointer, to HTfind().
 *
 * Correct behaviour: an object type the toolkit does not know is refused:
 * swmm_getCount returns no count (0 as in 5.2.4, or a negative error code)
 * and swmm_getIndex returns a negative value. Types that exist keep working
 * (the deck has 1 gage, 1 subcatchment, 2 nodes, 1 link). Under
 * AddressSanitizer the out-of-bounds read stops the test (CRASH).
 */
#include <stdio.h>
#include "swmm5.h"

int main(void)
{
    static const int badType[] = { 18, 50, 99 };
    int i, n, idx, bad = 0;

    if (swmm_open("CRASH-14_model.inp", "CRASH-14.rpt", "CRASH-14.out"))
    {
        printf("FAIL: swmm_open failed\n");
        return 1;
    }

    /* object types that exist */
    n = swmm_getCount(swmm_NODE);
    idx = swmm_getIndex(swmm_NODE, "O1");
    printf("swmm_getCount(swmm_NODE)        = %d (deck has 2)\n", n);
    printf("swmm_getIndex(swmm_NODE, \"O1\")  = %d (expected 1)\n", idx);
    if (n != 2 || idx != 1) bad++;

    /* object types that do not exist */
    for (i = 0; i < 3; i++)
    {
        printf("swmm_getCount(%d) ...\n", badType[i]);
        fflush(stdout);
        n = swmm_getCount(badType[i]);
        printf("swmm_getCount(%d)             = %d\n", badType[i], n);
        if (n > 0) bad++;

        printf("swmm_getIndex(%d, \"J1\") ...\n", badType[i]);
        fflush(stdout);
        idx = swmm_getIndex(badType[i], "J1");
        printf("swmm_getIndex(%d, \"J1\")       = %d\n", badType[i], idx);
        if (idx >= 0) bad++;
    }
    swmm_close();

    if (bad)
    {
        printf("FAIL: %d calls returned a count or an index for an unknown "
               "object type (or a wrong one for a known type)\n", bad);
        return 1;
    }
    printf("PASS: unknown object types are refused and known types still work\n");
    return 0;
}
