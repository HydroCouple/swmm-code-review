/*
 * BND-13: a CUSTOM inlet with a RATING curve ignores the Qmax capture limit
 * of [INLET_USAGE].
 *
 * The input reference defines Qmax as the "maximum flow that the inlet can
 * capture" and only 0 as "no flow restriction"; it makes no exception for
 * custom inlets. The deck gives a depth-rated custom inlet Qmax = 0.5 cfs on
 * a one-sided street carrying 3 cfs. The rating curve alone would capture
 * about 2 cfs at the street depth, so the correct capture is the limit,
 * 0.5 cfs.
 *
 * Measured capture = Q(S1) - Q(S2), the drop in street flow across the inlet,
 * at the end of the 3-hour run with a constant inflow (steady state).
 * Tolerance 0.02 cfs (4% of the limit); the defect captures about 2 cfs.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    const double qmax = 0.5;
    double elapsed = 0.0, q1 = 0, q2 = 0, d = 0, capture;
    int err, s1, s2, b;

    err = swmm_open("BND-13_rating-inlet-maxflow.inp", "BND-13.rpt", "BND-13.out");
    if (!err) err = swmm_start(1);
    if (err) { printf("FAIL: could not start the run (error %d)\n", err); return 1; }
    s1 = swmm_getIndex(swmm_LINK, "S1");
    s2 = swmm_getIndex(swmm_LINK, "S2");
    b  = swmm_getIndex(swmm_NODE, "B");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        q1 = swmm_getValue(swmm_LINK_FLOW, s1);
        q2 = swmm_getValue(swmm_LINK_FLOW, s2);
        d  = swmm_getValue(swmm_NODE_DEPTH, b);
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    capture = q1 - q2;
    printf("End of run:\n");
    printf("  depth at inlet node B      %7.4f ft (rating curve: %.2f cfs)\n", d, d * 10.0);
    printf("  street flow S1 / S2        %7.3f / %.3f cfs\n", q1, q2);
    printf("  captured flow              %7.3f cfs\n", capture);
    printf("  Qmax in [INLET_USAGE]      %7.3f cfs\n", qmax);
    if (fabs(capture - qmax) > 0.02)
    {
        printf("FAIL: the rating-curve inlet captures %.3f cfs, more than its Qmax of %.3f cfs\n",
               capture, qmax);
        return 1;
    }
    printf("PASS: the rating-curve inlet captures its Qmax of %.3f cfs (%.3f cfs)\n", qmax, capture);
    return 0;
}
