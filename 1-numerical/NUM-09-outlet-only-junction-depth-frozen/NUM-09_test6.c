/*
 * NUM-09 for 6.0.0: the same check as NUM-09_test.c through the 6.0.0 C API.
 *
 * SWMMEngine::initGeometry() deliberately reproduces the legacy crown of an
 * outlet (yFull = TINY), and DWSolver::setNodeDepth() takes the dQ/dH
 * surcharge branch, where sumdqdh = 0 gives dy = 0. J1's depth freezes.
 *
 * Correct behaviour: J1 rises to 4.0 ft, where the outlet rating
 * q = 1.0*h^0.5 passes the 2 cfs inflow, and the routing continuity error is
 * ~0. Tolerances as in NUM-09_test.c (0.05 ft, 1%).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    double elapsed = 0.0, depth = 0.0, qOut = 0.0, qIn = 0.0, contErr = 0.0;
    int rc, j1, ol1, nextReport = 15;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "NUM-09_outlet-junction.inp", "NUM-09_6.rpt",
                          "NUM-09_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1  = swmm_node_index(e, "J1");
    ol1 = swmm_link_index(e, "OL1");
    printf("  time  J1 inflow  J1 depth  OL1 flow\n");
    printf(" (min)      (cfs)      (ft)     (cfs)\n");
    while (!rc)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        swmm_node_get_inflow(e, j1, &qIn);
        swmm_node_get_depth(e, j1, &depth);
        swmm_link_get_flow(e, ol1, &qOut);
        if (elapsed * 1440.0 >= nextReport - 1e-6)
        {
            if (nextReport <= 60 || (nextReport % 60 == 0 && nextReport < 180))
                printf("%6.0f %10.3f %9.3f %9.3f\n",
                       elapsed * 1440.0, qIn, depth, qOut);
            nextReport += 15;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_get_routing_continuity_error(e, &contErr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }
    contErr *= 100.0;   /* fraction -> % */

    printf("Expected J1 depth: 4.000 ft (outlet passes 2 cfs)\n");
    printf("Routing continuity error: %.3f %%\n", contErr);
    if (fabs(depth - 4.0) > 0.05 || fabs(contErr) > 1.0)
    {
        printf("FAIL: J1 ends at %.3f ft instead of 4.000 ft, the outlet passes "
               "%.3f of 2 cfs, continuity error %.3f %%\n", depth, qOut, contErr);
        return 1;
    }
    printf("PASS: J1 fills to %.3f ft where the outlet passes the 2 cfs inflow; "
           "continuity error %.3f %%\n", depth, contErr);
    return 0;
}
