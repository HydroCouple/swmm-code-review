/*
 * NUM-23: in SI units with a width factor, the bank stations are not found.
 *
 * T1 has stations 0/10/30/40 m, banks at 10 and 30 m and Wfactor 1.5, so its
 * stations are 0/15/45/60 m with banks at 15 and 45 m. T2 is that section
 * entered directly. n = 0.10 on the overbanks and 0.03 in the channel.
 *
 * Check 1 (analytic): at full depth (2 m) the section is two overbank
 * triangles (A = 15 m2, P = 15.13 m) and a 30 m channel (A = 60 m2,
 * P = 30 m). SWMM's full flow for a transect is the sum of the subsection
 * Manning flows, Qfull = sqrt(S) * sum(1/n_i * A_i * R_i^(2/3)), 109.8 m3/s
 * at S = 0.001. C1 and C2 must have this within 2 % (5.2.4's 1.49 constant,
 * NUM-57, costs 0.3 %); the defect gives -66 %.
 * Check 2 (documented equivalence: Wfactor multiplies the stations): with
 * 5 m3/s in each, the steady depths at J1 and J2 must agree to 1 %.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double qfull[2], depth[2] = {0}, slope = 0, elapsed = 0, expect;
    int i, err, ln[2], nd[2];

    err = swmm_open("NUM-23_si-wfactor.inp", "NUM-23.rpt", "NUM-23.out");
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

    expect = sqrt(slope) * (2.0 / 0.10 * 15.0 * pow(15.0 / sqrt(229.0), 2.0 / 3.0)
                            + 1.0 / 0.03 * 60.0 * pow(60.0 / 30.0, 2.0 / 3.0));

    printf("Conduit  Transect                       Full flow (m3/s)  Depth at 5 m3/s (m)\n");
    printf("C1       T1 0/10/30/40 m, Wfactor 1.5       %8.2f           %6.3f\n", qfull[0], depth[0]);
    printf("C2       T2 0/15/45/60 m                    %8.2f           %6.3f\n", qfull[1], depth[1]);
    printf("Analytic full flow: %.2f m3/s\n", expect);

    if (fabs(qfull[0] / expect - 1.0) > 0.02 || fabs(qfull[1] / expect - 1.0) > 0.02 ||
        fabs(depth[0] / depth[1] - 1.0) > 0.01)
    {
        printf("FAIL: with Wfactor 1.5 the full flow is %.2f m3/s (%+.0f %% from %.2f) and the "
               "depth %.3f m vs %.3f m for the same stations entered directly\n", qfull[0],
               100.0 * (qfull[0] / expect - 1.0), expect, depth[0], depth[1]);
        return 1;
    }
    printf("PASS: the bank stations are found and Wfactor gives the same section as scaled stations\n");
    return 0;
}
