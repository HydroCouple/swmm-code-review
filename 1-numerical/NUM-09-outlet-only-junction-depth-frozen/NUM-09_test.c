/*
 * NUM-09: a junction whose only links are outlets never changes depth under
 * dynamic wave routing.
 *
 * An outlet has a DUMMY cross-section with yFull = TINY (1e-6 ft), so
 * dynwave_init() gives J1 a crown 1e-6 ft above its invert. setNodeDepth()
 * then treats J1 as EXTRAN-surcharged at any depth and updates it with
 * dy = dQ / sumdqdh. Outlets leave dqdh = 0, so sumdqdh = 0, dy = 0, and the
 * depth stays where the first step put it while the difference between
 * inflow and outflow is lost.
 *
 * Deck: 2 cfs into junction J1, which starts dry and drains only through
 * outlet OL1 with the rating q = 1.0*h^0.5 (h = J1 depth). 3 h, 30 s steps.
 *
 * Correct behaviour: J1 fills until the outlet passes the inflow,
 * 1.0*h^0.5 = 2 -> h = 4.0 ft (with SWMM's 12.566 ft2 minimum junction area
 * and dq/dh = 0.25 cfs/ft the time constant is ~50 s, so it is there long
 * before 3 h), and the routing continuity error is ~0.
 *
 * Tolerances: final depth within 0.05 ft of 4.0 ft and |continuity error|
 * below 1%. Junction storage is not booked as volume in SWMM, so filling
 * J1 to 4 ft leaves ~50 ft3 of 21,600 ft3 (0.2%) unbooked; the defect
 * freezes J1 at 2.39 ft and loses ~22% of the inflow.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double elapsed = 0.0, depth = 0.0, qOut = 0.0, qIn = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, j1, ol1, nextReport = 15;

    err = swmm_open("NUM-09_outlet-junction.inp", "NUM-09.rpt", "NUM-09.out");
    if (!err) err = swmm_start(1);
    j1  = swmm_getIndex(swmm_NODE, "J1");
    ol1 = swmm_getIndex(swmm_LINK, "OL1");
    printf("  time  J1 inflow  J1 depth  OL1 flow\n");
    printf(" (min)      (cfs)      (ft)     (cfs)\n");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        qIn   = swmm_getValue(swmm_NODE_INFLOW, j1);
        depth = swmm_getValue(swmm_NODE_DEPTH, j1);
        qOut  = swmm_getValue(swmm_LINK_FLOW, ol1);
        if (elapsed * 1440.0 >= nextReport - 1e-6)
        {
            if (nextReport <= 60 || (nextReport % 60 == 0 && nextReport < 180))
                printf("%6.0f %10.3f %9.3f %9.3f\n",
                       elapsed * 1440.0, qIn, depth, qOut);
            nextReport += 15;
        }
    }
    swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    printf("Expected J1 depth: 4.000 ft (outlet passes 2 cfs)\n");
    printf("Routing continuity error: %.3f %%\n", flowErr);
    if (fabs(depth - 4.0) > 0.05 || fabs(flowErr) > 1.0)
    {
        printf("FAIL: J1 ends at %.3f ft instead of 4.000 ft, the outlet passes "
               "%.3f of 2 cfs, continuity error %.3f %%\n", depth, qOut, flowErr);
        return 1;
    }
    printf("PASS: J1 fills to %.3f ft where the outlet passes the 2 cfs inflow; "
           "continuity error %.3f %%\n", depth, flowErr);
    return 0;
}
