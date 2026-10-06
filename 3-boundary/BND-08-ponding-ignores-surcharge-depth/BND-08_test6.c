/*
 * BND-08 for 6.0.0: the same check as BND-08_test.c through the 6.0.0 C API.
 *
 * DWSolver::setNodeDepth() and commitNodeDepthState() (DynamicWave.cpp) keep
 * the legacy rule: a node that can pond is ponded above its full depth, and
 * its surcharge depth is added to the flood threshold only when it cannot
 * pond.
 *
 * Correct behaviour (input manual: Apond applies "once water depth exceeds
 * Ymax + Ysur"): whenever J1 holds more than 1 ft3, its depth is at least
 * 15 ft (0.01 ft tolerance).
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    double elapsed = 0.0, y, v, q, yMinPonded = 1.0e9, vMax = 0.0;
    int rc, j1, c1, nextReport = 5;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "BND-08_bolted-cover-pond.inp", "BND-08_6.rpt",
                          "BND-08_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    c1 = swmm_link_index(e, "C1");
    printf("  time   J1 depth   J1 ponded vol   C1 flow\n");
    printf(" (min)       (ft)           (ft3)     (cfs)\n");
    while (!rc)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        swmm_node_get_depth(e, j1, &y);
        swmm_node_get_volume(e, j1, &v);
        swmm_link_get_flow(e, c1, &q);
        if (v > 1.0 && y < yMinPonded) yMinPonded = y;
        if (v > vMax) vMax = v;
        if (elapsed * 1440.0 >= nextReport - 1e-6 && nextReport < 60)
        {
            printf("%6d %10.2f %15.0f %9.2f\n", nextReport, y, v, q);
            nextReport += (nextReport < 20) ? 5 : 10;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

    if (vMax <= 1.0)
        printf("J1 never ponds\n");
    else
        printf("Lowest J1 depth while ponded: %.2f ft (Ymax + Ysur = 15.00 ft)\n",
               yMinPonded);
    if (vMax > 1.0 && yMinPonded < 15.0 - 0.01)
    {
        printf("FAIL: J1 ponds at a depth of %.2f ft, below Ymax + Ysur = 15 ft "
               "(up to %.0f ft3 ponded)\n", yMinPonded, vMax);
        return 1;
    }
    printf("PASS: J1 pressurizes to Ymax + Ysur before any water ponds\n");
    return 0;
}
