/*
 * IO-25: the input reference gives the [STREETS] gutter depression 'a' (and
 * the [INLET_USAGE] / [INLET_JUNCTIONS] 'aLocal') in "in or mm"; the engine
 * reads them in ft or m, like every other street length.
 *
 * This test establishes which unit the engine uses, against a published
 * answer: HEC-22 (3rd ed.) Example 4-9, a 9.84-ft (3 m) curb opening on a
 * street with Sx = 0.02, SL = 0.01, n = 0.016 and a 2-ft gutter depressed
 * 1 inch, carrying 1.77 cfs (0.05 m3/s). HEC-22's answer is 1.55 cfs
 * (0.044 m3/s) captured. EPA's own verification deck
 * street_curb_inlet_9a-CFS.inp enters that depression as 0.0833.
 *
 * Street A enters a = 0.0833 (1 inch in feet), street B a = 1 (1 inch per
 * the input reference). Capture = drop in street flow across the inlet at
 * the end of the 3-hour run (steady state).
 * PASS when street A gives HEC-22's 1.55 cfs (tolerance 0.03 cfs), i.e. the
 * engine reads 'a' in feet and the input reference is wrong. Street B shows
 * what a user following the manual gets: a 1-ft deep gutter.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    const char *up[2] = {"SA1", "SB1"}, *dn[2] = {"SA2", "SB2"};
    double elapsed = 0.0, qu[2] = {0}, qd[2] = {0}, cap[2];
    int iu[2], id[2], k, err;

    err = swmm_open("IO-25_hec22-example-4-9.inp", "IO-25.rpt", "IO-25.out");
    if (!err) err = swmm_start(1);
    if (err) { printf("FAIL: could not start the run (error %d)\n", err); return 1; }
    for (k = 0; k < 2; k++)
    {
        iu[k] = swmm_getIndex(swmm_LINK, up[k]);
        id[k] = swmm_getIndex(swmm_LINK, dn[k]);
    }
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        for (k = 0; k < 2; k++)
        {
            qu[k] = swmm_getValue(swmm_LINK_FLOW, iu[k]);
            qd[k] = swmm_getValue(swmm_LINK_FLOW, id[k]);
        }
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    for (k = 0; k < 2; k++) cap[k] = qu[k] - qd[k];
    printf("HEC-22 Example 4-9, depressed gutter: 1.55 cfs of 1.77 cfs captured\n");
    printf("Street  a entered       Q (cfs)  captured (cfs)  capture %%\n");
    printf("A       0.0833 (ft)     %7.3f  %14.3f  %9.2f\n", qu[0], cap[0], 100.0 * cap[0] / qu[0]);
    printf("B       1      (in?)    %7.3f  %14.3f  %9.2f\n", qu[1], cap[1], 100.0 * cap[1] / qu[1]);
    if (fabs(cap[0] - 1.55) > 0.03)
    {
        printf("FAIL: a = 0.0833 does not reproduce HEC-22's 1.55 cfs (%.3f cfs)\n", cap[0]);
        return 1;
    }
    printf("PASS: the engine reads the gutter depression in feet (a = 0.0833 gives HEC-22's "
           "%.3f cfs); a = 1 is read as a 1-ft gutter (%.3f cfs captured)\n", cap[0], cap[1]);
    return 0;
}
