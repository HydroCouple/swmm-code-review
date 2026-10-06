/*
 * NUM-22: an NC line of zeros inherits the previous transect's meander-adjusted n.
 *
 * The input manual: on an NC line, "use 0 if no change from previous NC line".
 * T1 has n = 0.05 / 0.05 / 0.03 (left / right overbank / channel) and a
 * meander factor of 2.0. T2 follows with "NC 0 0 0" and no meander factor, so
 * it must use 0.05 / 0.05 / 0.03 as written. T3 is the same transect with
 * those values written out.
 *
 * Check 1 (analytic): at full depth (5 ft) the stations 0-10-30-40 give two
 * overbank triangles (A = 25 ft2, P = 11.18 ft each) and a 20 ft channel
 * (A = 100 ft2, P = 20 ft). SWMM's full flow for a transect is the sum of the
 * subsection Manning flows, Qfull = sqrt(S) * sum(1.486/n_i * A_i * R_i^(2/3)),
 * 538.4 cfs at S = 0.001. C2 and C3 must have this within 2 % (5.2.4's 1.49
 * constant, NUM-57, costs 0.3 %); the defect gives -25 %.
 * Check 2 (documented equivalence): with 20 cfs in each, the steady depths at
 * J2 and J3 must agree to 1 %; the defect makes J2 about 20 % deeper.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    const char *links[3] = {"C1", "C2", "C3"}, *nodes[3] = {"J1", "J2", "J3"};
    double qfull[3], slope = 0, depth[3] = {0}, elapsed = 0, expect, sumk;
    int i, err, ln[3], nd[3];

    err = swmm_open("NUM-22_transects.inp", "NUM-22.rpt", "NUM-22.out");
    if (!err) err = swmm_start(1);
    for (i = 0; !err && i < 3; i++)
    {
        ln[i] = swmm_getIndex(swmm_LINK, links[i]);
        nd[i] = swmm_getIndex(swmm_NODE, nodes[i]);
        qfull[i] = swmm_getValue(swmm_LINK_FULLFLOW, ln[i]);
    }
    if (!err) slope = swmm_getValue(swmm_LINK_SLOPE, ln[1]);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        for (i = 0; i < 3; i++) depth[i] = swmm_getValue(swmm_NODE_DEPTH, nd[i]);
    }
    swmm_end();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    /* subsection conveyances at full depth, n = 0.05 / 0.03 / 0.05 */
    sumk = 2.0 * 1.486 / 0.05 * 25.0 * pow(25.0 / sqrt(125.0), 2.0 / 3.0)
         + 1.486 / 0.03 * 100.0 * pow(100.0 / 20.0, 2.0 / 3.0);
    expect = sumk * sqrt(slope);

    printf("Conduit  Transect                 Full flow (cfs)  Depth at 20 cfs (ft)\n");
    printf("C1       T1 meander 2.0              %8.2f          %6.3f\n", qfull[0], depth[0]);
    printf("C2       T2 NC 0 0 0 (inherits)      %8.2f          %6.3f\n", qfull[1], depth[1]);
    printf("C3       T3 NC 0.05 0.05 0.03        %8.2f          %6.3f\n", qfull[2], depth[2]);
    printf("Analytic full flow with n = 0.05/0.05/0.03: %.2f cfs\n", expect);

    if (fabs(qfull[1] / expect - 1.0) > 0.02 || fabs(qfull[2] / expect - 1.0) > 0.02 ||
        fabs(depth[1] / depth[2] - 1.0) > 0.01)
    {
        printf("FAIL: T2 (NC 0 0 0) has full flow %.2f cfs (%+.0f %% from %.2f) and depth "
               "%.3f ft vs %.3f ft for the same n written out\n", qfull[1],
               100.0 * (qfull[1] / expect - 1.0), expect, depth[1], depth[2]);
        return 1;
    }
    printf("PASS: NC 0 0 0 reuses the n values as written, not the meander-adjusted ones\n");
    return 0;
}
