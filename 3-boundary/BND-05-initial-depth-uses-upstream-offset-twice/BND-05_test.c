/*
 * BND-05: initNodeDepths() adds a conduit's inlet offset to the initial depth
 * of both of its end nodes.
 *
 * Deck: J1 -> C1 -> J2 -> C2 -> free outfall O1. Both conduits have an
 * initial flow of 3 cfs, so the engine starts them at normal depth. C1 enters
 * J1 4 ft above J1's invert (inlet offset 4 ft) and leaves into J2 at J2's
 * invert (outlet offset 0). C2 has no offsets. Neither junction has a
 * user-supplied initial depth, so the engine seeds them from the conduits.
 *
 * Correct behaviour, from the geometry: at J2 the water surface of C1's
 * downstream end is C1 depth + C1 outlet offset (0 ft) above J2's invert, and
 * the water surface of C2's upstream end is C2 depth + C2 inlet offset (0 ft).
 * J2's initial depth must lie between those two levels (the engine averages
 * them). The bug adds C1's 4 ft inlet offset instead, which puts J2 about 2 ft
 * above both conduits. The tolerance (0.05 ft) is far below that 2 ft error.
 *
 * Consequence checked as well: the false head at J2 drives a start-up surge
 * whose water is not in any initial storage term. The routing continuity
 * error must stay within 5% (it is about -20% with the bug and about 1% when
 * J2 starts at the right depth).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#define C1_OUTLET_OFFSET 0.0   /* ft, from the deck */
#define C2_INLET_OFFSET  0.0   /* ft, from the deck */

int main(void)
{
    double elapsed = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    double yJ1, yJ2, yC1, yC2, lo, hi;
    int err, j1, j2, c1, c2, depthOk, massOk;

    err = swmm_open("BND-05_offset-inlet.inp", "BND-05.rpt", "BND-05.out");
    if (!err) err = swmm_start(1);
    if (err)
    {
        printf("FAIL: could not open/start the model (error %d)\n", err);
        return 1;
    }
    j1 = swmm_getIndex(swmm_NODE, "J1");
    j2 = swmm_getIndex(swmm_NODE, "J2");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    c2 = swmm_getIndex(swmm_LINK, "C2");

    /* initial state, before the first routing step */
    yJ1 = swmm_getValue(swmm_NODE_DEPTH, j1);
    yJ2 = swmm_getValue(swmm_NODE_DEPTH, j2);
    yC1 = swmm_getValue(swmm_LINK_DEPTH, c1);
    yC2 = swmm_getValue(swmm_LINK_DEPTH, c2);

    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();

    lo = fmin(yC1 + C1_OUTLET_OFFSET, yC2 + C2_INLET_OFFSET);
    hi = fmax(yC1 + C1_OUTLET_OFFSET, yC2 + C2_INLET_OFFSET);

    printf("Initial state (ft)\n");
    printf("  C1 depth (normal depth, 3 cfs)        %6.3f\n", yC1);
    printf("  C2 depth (normal depth, 3 cfs)        %6.3f\n", yC2);
    printf("  J1 depth                              %6.3f\n", yJ1);
    printf("  J2 depth                              %6.3f\n", yJ2);
    printf("  water levels of C1/C2 ends at J2      %6.3f .. %6.3f\n", lo, hi);
    printf("Routing continuity error (%%)          %7.3f\n", flowErr);

    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    /* 0.05 ft tolerance: the bug puts J2 about 2 ft outside this range */
    depthOk = (yJ2 >= lo - 0.05 && yJ2 <= hi + 0.05);
    /* 5%: the bug gives about -20%, the corrected start about 1% */
    massOk = (fabs(flowErr) <= 5.0);

    if (!depthOk || !massOk)
    {
        printf("FAIL: J2 starts at %.3f ft (water levels of its conduits: %.3f-%.3f ft)"
               "%s; routing continuity error %.3f%%%s\n",
               yJ2, lo, hi, depthOk ? "" : ", outside that range",
               flowErr, massOk ? "" : ", beyond 5%");
        return 1;
    }
    printf("PASS: J2 starts between the water levels of its conduits and "
           "routing continuity is %.3f%%\n", flowErr);
    return 0;
}
