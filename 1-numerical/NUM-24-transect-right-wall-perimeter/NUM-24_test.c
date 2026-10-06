/*
 * NUM-24: a transect and its mirror image must have the same hydraulics.
 *
 * T1 (GR 101 0  100 2  100 8  105 10) and T2 (GR 105 0  100 2  100 8  101 10)
 * are the same channel reflected left to right; n = 0.03 throughout. SWMM
 * extends the lower end with a vertical wall up to the full height (5 ft).
 * Reflection changes nothing physical, so the full flow and the depth at a
 * given flow must be the same.
 *
 * Check 1 (mirror symmetry): full flows within 0.5 % and steady depths at
 * 60 cfs within 1 %. The defect gives a 19 % difference in full flow.
 * Check 2 (reference manual, Vol. II section 5.3: the added end station is a
 * segment like any other, so its wetted perimeter counts): at full depth
 * A = 44 ft2 and P = 4 (wall) + sqrt(5) + 6 + sqrt(29) = 17.62 ft, so
 * Qfull = 1.486/0.03 * A * (A/P)^(2/3) * sqrt(S) = 126.8 cfs at S = 0.001.
 * Both full flows must be within 1 % of it (5.2.4's 1.49 constant, NUM-57,
 * costs 0.3 %).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double qfull[2], depth[2] = {0}, slope = 0, elapsed = 0, a, p, expect;
    int i, err, ln[2], nd[2];

    err = swmm_open("NUM-24_mirror.inp", "NUM-24.rpt", "NUM-24.out");
    if (!err) err = swmm_start(1);
    if (!err)
    {
        ln[0] = swmm_getIndex(swmm_LINK, "C1");  ln[1] = swmm_getIndex(swmm_LINK, "C2");
        nd[0] = swmm_getIndex(swmm_NODE, "J1");  nd[1] = swmm_getIndex(swmm_NODE, "J2");
        for (i = 0; i < 2; i++) qfull[i] = swmm_getValue(swmm_LINK_FULLFLOW, ln[i]);
        slope = swmm_getValue(swmm_LINK_SLOPE, ln[0]);
    }
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        for (i = 0; i < 2; i++) depth[i] = swmm_getValue(swmm_NODE_DEPTH, nd[i]);
    }
    swmm_end();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    a = 44.0;
    p = 4.0 + sqrt(5.0) + 6.0 + sqrt(29.0);
    expect = 1.486 / 0.03 * a * pow(a / p, 2.0 / 3.0) * sqrt(slope);

    printf("Conduit  Transect                       Full flow (cfs)  Depth at 60 cfs (ft)\n");
    printf("C1       T1 low end on the left             %8.2f          %6.3f\n", qfull[0], depth[0]);
    printf("C2       T2 low end on the right            %8.2f          %6.3f\n", qfull[1], depth[1]);
    printf("Full flow with both end walls in the wetted perimeter: %.2f cfs\n", expect);

    if (fabs(qfull[1] / qfull[0] - 1.0) > 0.005 || fabs(depth[1] / depth[0] - 1.0) > 0.01 ||
        fabs(qfull[0] / expect - 1.0) > 0.01 || fabs(qfull[1] / expect - 1.0) > 0.01)
    {
        printf("FAIL: the mirror image has %+.1f %% full flow (%.2f vs %.2f cfs) and "
               "%+.1f %% depth at 60 cfs\n", 100.0 * (qfull[1] / qfull[0] - 1.0),
               qfull[1], qfull[0], 100.0 * (depth[1] / depth[0] - 1.0));
        return 1;
    }
    printf("PASS: a transect and its mirror image have the same full flow and depth\n");
    return 0;
}
