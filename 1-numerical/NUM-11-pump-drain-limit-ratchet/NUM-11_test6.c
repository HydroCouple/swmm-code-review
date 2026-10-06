/*
 * NUM-11 for 6.0.0: the same check as NUM-11_test.c through the 6.0.0 C API.
 *
 * StructureSolver (HydStructures.cpp) keeps the legacy limit on purpose: its
 * comment says it divides by the raw node surface area "NO MinSurfArea
 * floor" for parity. With a free-falling inlet conduit that area is 0, so
 * the pump is pinned to the inflow and J1 never drains.
 *
 * Correct behaviour: from 0:15 on, J1 is near empty and P1 pumps the 1 cfs
 * inflow. Tolerance as in NUM-11_test.c (0.5 ft).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    double elapsed = 0.0, worst = 0.0, contErr = 0.0, y, qc, qp;
    int rc, j1, p1, c0, k = 0;
    const double tRep[] = {5, 10, 12, 15, 30, 59};   /* minutes */
    const int nRep = 6;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "NUM-11_junction-wet-well.inp", "NUM-11_6.rpt",
                          "NUM-11_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    p1 = swmm_link_index(e, "P1");
    c0 = swmm_link_index(e, "C0");
    printf("  time   C0 inflow  J1 depth   P1 flow\n");
    printf(" (min)       (cfs)      (ft)     (cfs)\n");
    while (!rc)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        if (k < nRep && elapsed * 1440.0 >= tRep[k] - 1e-6)
        {
            swmm_node_get_depth(e, j1, &y);
            swmm_link_get_flow(e, c0, &qc);
            swmm_link_get_flow(e, p1, &qp);
            printf("%6.0f %11.3f %9.3f %9.3f\n", tRep[k], qc, y, qp);
            if (tRep[k] >= 15 && y > worst) worst = y;
            k++;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_get_routing_continuity_error(e, &contErr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

    printf("Routing continuity error: %.3f %%\n", 100.0 * contErr);
    if (worst > 0.5)
    {
        printf("FAIL: with 1 cfs inflow and a 5 cfs pump, J1 stays at %.3f ft "
               "instead of being drawn down\n", worst);
        return 1;
    }
    printf("PASS: the pump draws J1 down once the inflow drops below its "
           "capacity (J1 at most %.3f ft from 0:15 on)\n", worst);
    return 0;
}
