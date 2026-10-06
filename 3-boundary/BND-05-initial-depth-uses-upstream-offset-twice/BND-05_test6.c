/*
 * BND-05 for 6.0.0: the initNodeDepths port in SWMMEngine.cpp adds a
 * conduit's inlet offset to the initial depth of both of its end nodes.
 *
 * Same deck and checks as BND-05_test.c: J2's initial depth must lie between
 * the water levels of the conduit ends that meet there (C1 depth + outlet
 * offset 0, C2 depth + inlet offset 0), within 0.05 ft (the bug is about 2 ft
 * off), and the routing continuity error must stay within 5% (about -20% with
 * the bug).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

#define C1_OUTLET_OFFSET 0.0   /* ft, from the deck */
#define C2_INLET_OFFSET  0.0   /* ft, from the deck */

int main(void)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, yJ1 = 0, yJ2 = 0, yC1 = 0, yC2 = 0, lo, hi, cont = 0, flowErr;
    int rc, j1, j2, c1, c2, depthOk, massOk;

    rc = swmm_engine_open(e, "BND-05_offset-inlet.inp", "BND-05_6.rpt", "BND-05_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    if (rc)
    {
        printf("FAIL: could not open/start the model (error %d)\n", rc);
        return 1;
    }
    j1 = swmm_node_index(e, "J1");
    j2 = swmm_node_index(e, "J2");
    c1 = swmm_link_index(e, "C1");
    c2 = swmm_link_index(e, "C2");

    /* initial state, before the first routing step (CFS model: ft) */
    swmm_node_get_depth(e, j1, &yJ1);
    swmm_node_get_depth(e, j2, &yJ2);
    swmm_link_get_depth(e, c1, &yC1);
    swmm_link_get_depth(e, c2, &yC2);

    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_routing_continuity_error(e, &cont);   /* a fraction */
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    flowErr = 100.0 * cont;

    lo = fmin(yC1 + C1_OUTLET_OFFSET, yC2 + C2_INLET_OFFSET);
    hi = fmax(yC1 + C1_OUTLET_OFFSET, yC2 + C2_INLET_OFFSET);

    printf("Initial state (ft)\n");
    printf("  C1 depth (normal depth, 3 cfs)        %6.3f\n", yC1);
    printf("  C2 depth (normal depth, 3 cfs)        %6.3f\n", yC2);
    printf("  J1 depth                              %6.3f\n", yJ1);
    printf("  J2 depth                              %6.3f\n", yJ2);
    printf("  water levels of C1/C2 ends at J2      %6.3f .. %6.3f\n", lo, hi);
    printf("Routing continuity error (%%)          %7.3f\n", flowErr);

    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
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
