/*
 * NUM-59 for 6.0.0: a model with nodes but no links must still route its
 * inflows into storage (5.2.4 does not; 5.3.0 and 6.0.0 do).
 *
 * Correct behaviour, as in NUM-59_test.c: S1, a sealed storage unit of
 * constant area 10,000 ft2 that receives 1 cfs for 6 h, ends with
 * 21,600 ft3 (2.16 ft); S2, the same unit with exfiltration, ends with less
 * than S1 but more than nothing; the flow routing continuity error is near
 * zero. Tolerances: S1 within 0.5 % of 21,600 ft3 and |error| < 0.5 %.
 * swmm_get_routing_continuity_error() returns a fraction (0.001 = 0.1 %).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    double t = 0.0, v1 = 0.0, v2 = 0.0, d1 = 0.0, d2 = 0.0, flowErr = 0.0;
    const double vIn = 1.0 * 6.0 * 3600.0;      /* ft3 */
    int rc, s1 = -1, s2 = -1, ok;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-59_storage-only.inp", "NUM-59_6.rpt", "NUM-59_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    if (!rc) { s1 = swmm_node_index(e, "S1"); s2 = swmm_node_index(e, "S2"); }
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    swmm_node_get_volume(e, s1, &v1);
    swmm_node_get_volume(e, s2, &v2);
    swmm_node_get_depth(e, s1, &d1);
    swmm_node_get_depth(e, s2, &d2);
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_routing_continuity_error(e, &flowErr);
    flowErr *= 100.0;
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    printf("Inflow volume per node        %10.1f ft3\n", vIn);
    printf("S1 (sealed) final volume      %10.1f ft3   depth %6.3f ft   (expected %.1f ft3, %.3f ft)\n",
           v1, d1, vIn, vIn / 10000.0);
    printf("S2 (exfiltrates) final volume %10.1f ft3   depth %6.3f ft   (expected between 0 and S1)\n",
           v2, d2);
    printf("Flow routing continuity error %10.3f %%\n", flowErr);
    printf("Error code                    %10d\n", rc);

    ok = !rc && fabs(v1 - vIn) < 0.005 * vIn && v2 > 0.0 && v2 < v1 && fabs(flowErr) < 0.5;
    if (!ok)
    {
        printf("FAIL: the inflow to the link-less model is not routed: S1 holds %.1f of %.1f ft3, "
               "continuity error %.3f %%\n", v1, vIn, flowErr);
        return 1;
    }
    printf("PASS: the storage units hold the inflow they received (continuity error %.3f %%)\n",
           flowErr);
    return 0;
}
