/*
 * IO-25 for 6.0.0: same check as IO-25_test.c through the 6.0.0 C API.
 * 6.0.0 also divides the depression by the length conversion factor
 * (PostParseResolver.cpp, Inlet.cpp), so it reads feet / metres too.
 *
 * The input reference gives the [STREETS] gutter depression 'a' (and
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
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    const char *up[2] = {"SA1", "SB1"}, *dn[2] = {"SA2", "SB2"};
    double t = 0.0, qu[2] = {0}, qd[2] = {0}, cap[2];
    int iu[2], id[2], k, err;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "IO-25_hec22-example-4-9.inp", "IO-25_6.rpt", "IO-25_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (err) { printf("FAIL: could not start the run (error %d)\n", err); return 1; }
    for (k = 0; k < 2; k++)
    {
        iu[k] = swmm_link_index(e, up[k]);
        id[k] = swmm_link_index(e, dn[k]);
    }
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        for (k = 0; k < 2; k++)
        {
            swmm_link_get_flow(e, iu[k], &qu[k]);
            swmm_link_get_flow(e, id[k], &qd[k]);
        }
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
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
