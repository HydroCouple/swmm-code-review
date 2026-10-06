/*
 * CON-02 for 6.0.0: the same check as CON-02_test.c through the 6.0.0 C API.
 *
 * DWSolver::setNodeDepth() (DynamicWave.cpp) floors every non-virtual node's
 * surface area at MIN_SURFAREA, storage units included, while their volume
 * comes from the storage curve.
 *
 * Correct behaviour: S1 ends holding the ~30 ft3 that entered at ~7.5 ft
 * over its 4 ft2. Tolerances as in CON-02_test.c (0.5 ft3, 0.05 ft).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    double elapsed = 0.0, tPrev = 0.0, dt, q, qPrev = 0.0, vIn = 0.0;
    double y = 0.0, v = 0.0, contErr = 0.0;
    int rc, s1, nextReport = 2;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "CON-02_small-wet-well.inp", "CON-02_6.rpt",
                          "CON-02_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    s1 = swmm_node_index(e, "S1");
    printf("  time   inflow vol   S1 depth  S1 volume\n");
    printf(" (min)        (ft3)       (ft)      (ft3)\n");
    while (!rc)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        dt = (elapsed - tPrev) * 86400.0;
        tPrev = elapsed;
        swmm_node_get_inflow(e, s1, &q);                       /* cfs */
        vIn += 0.5 * (qPrev + q) * dt;                         /* ft3 */
        qPrev = q;
        swmm_node_get_depth(e, s1, &y);
        swmm_node_get_volume(e, s1, &v);
        if (elapsed * 1440.0 >= nextReport - 1e-6 && nextReport < 20)
        {
            printf("%6d %12.2f %10.3f %10.2f\n", nextReport, vIn, y, v);
            nextReport += (nextReport < 10) ? 2 : 5;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_get_routing_continuity_error(e, &contErr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

    printf("Expected at the end: volume %.2f ft3, depth %.3f ft (4 ft2)\n",
           vIn, vIn / 4.0);
    printf("Routing continuity error: %.3f %%\n", 100.0 * contErr);
    if (fabs(v - vIn) > 0.5 || fabs(y - vIn / 4.0) > 0.05)
    {
        printf("FAIL: S1 ends at %.3f ft holding %.2f ft3, but %.2f ft3 entered "
               "(%.3f ft over its 4 ft2)\n", y, v, vIn, vIn / 4.0);
        return 1;
    }
    printf("PASS: S1 holds the %.2f ft3 that entered, at %.3f ft\n", v, y);
    return 0;
}
