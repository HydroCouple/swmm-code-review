/*
 * NUM-59: in a model with nodes but no links, 5.2.4 books every inflow but
 * never routes it.
 *
 * routeFlow() only calls flowrout_execute() when Nobjects[LINK] > 0, while
 * the inflows are still added to the mass balance, so storage never fills
 * and the whole inflow shows up as continuity error.
 *
 * Correct behaviour (conservation of volume): S1, a sealed storage unit of
 * constant area 10,000 ft2 that receives 1 cfs for 6 h, ends with
 * 21,600 ft3 (2.16 ft). S2, the same unit with exfiltration, ends with less
 * than S1 but more than nothing, and the flow routing continuity error is
 * near zero. Tolerances: S1 within 0.5 % of 21,600 ft3 and |error| < 0.5 %;
 * the bug gives 0 ft3 and 100 %.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, v1 = 0.0, v2 = 0.0, d1 = 0.0, d2 = 0.0;
    float runoffErr = 0.0f, flowErr = 0.0f, qualErr = 0.0f;
    const double vIn = 1.0 * 6.0 * 3600.0;      /* ft3 */
    int err, s1, s2, ok;

    err = swmm_open("NUM-59_storage-only.inp", "NUM-59.rpt", "NUM-59.out");
    if (!err) err = swmm_start(1);
    s1 = swmm_getIndex(swmm_NODE, "S1");
    s2 = swmm_getIndex(swmm_NODE, "S2");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    v1 = swmm_getValue(swmm_NODE_VOLUME, s1);
    v2 = swmm_getValue(swmm_NODE_VOLUME, s2);
    d1 = swmm_getValue(swmm_NODE_DEPTH, s1);
    d2 = swmm_getValue(swmm_NODE_DEPTH, s2);
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();

    printf("Inflow volume per node        %10.1f ft3\n", vIn);
    printf("S1 (sealed) final volume      %10.1f ft3   depth %6.3f ft   (expected %.1f ft3, %.3f ft)\n",
           v1, d1, vIn, vIn / 10000.0);
    printf("S2 (exfiltrates) final volume %10.1f ft3   depth %6.3f ft   (expected between 0 and S1)\n",
           v2, d2);
    printf("Flow routing continuity error %10.3f %%\n", flowErr);
    printf("Error code                    %10d\n", err);

    ok = !err && fabs(v1 - vIn) < 0.005 * vIn && v2 > 0.0 && v2 < v1 && fabs(flowErr) < 0.5;
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
