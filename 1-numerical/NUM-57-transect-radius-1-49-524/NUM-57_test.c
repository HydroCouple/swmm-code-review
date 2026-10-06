/*
 * NUM-57: 5.2.4 builds the transect hydraulic-radius table with 1.49 while the
 * flows it is built from use PHI = 1.486.
 *
 * The transect is a plain rectangle, 20 ft wide and 5 ft deep, with one n
 * (0.03) and no overbanks. Its full hydraulic radius is A/P = 100/30 =
 * 3.333 ft, and its full flow is 1.486/n * A * (A/P)^(2/3) * sqrt(S), the
 * same as a RECT_OPEN 5 x 20 conduit (SWMM uses 1.486 for Manning's
 * equation everywhere else).
 *
 * The test reads the conduit's full flow, compares it with that value and
 * backs out the full hydraulic radius, R = (Qfull*n / (1.486*A*sqrt(S)))^1.5.
 * Tolerance 0.05 %: the toolkit returns the full flow in double precision,
 * so a correct engine is exact to rounding; the defect is -0.27 % in flow and
 * -0.40 % in R.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    const double n = 0.03, a = 100.0, p = 30.0;
    double qfull = 0, slope = 0, expect, r;
    int err, k;

    err = swmm_open("NUM-57_rect-transect.inp", "NUM-57.rpt", "NUM-57.out");
    if (!err) err = swmm_start(0);
    if (!err)
    {
        k = swmm_getIndex(swmm_LINK, "C1");
        qfull = swmm_getValue(swmm_LINK_FULLFLOW, k);
        slope = swmm_getValue(swmm_LINK_SLOPE, k);
    }
    swmm_end();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    expect = 1.486 / n * a * pow(a / p, 2.0 / 3.0) * sqrt(slope);
    r = pow(qfull * n / (1.486 * a * sqrt(slope)), 1.5);

    printf("                          engine      exact\n");
    printf("Full flow (cfs)        %10.4f %10.4f   (%+.3f %%)\n", qfull, expect, 100.0 * (qfull / expect - 1.0));
    printf("Full hyd. radius (ft)  %10.4f %10.4f   (%+.3f %%)\n", r, a / p, 100.0 * (r / (a / p) - 1.0));

    if (fabs(qfull / expect - 1.0) > 5.0e-4)
    {
        printf("FAIL: full flow %.4f cfs is %+.2f %% from Manning's equation (%.4f cfs); "
               "hydraulic radius %.4f ft instead of %.4f ft\n", qfull,
               100.0 * (qfull / expect - 1.0), expect, r, a / p);
        return 1;
    }
    printf("PASS: the transect's full flow and hydraulic radius follow Manning's equation with 1.486\n");
    return 0;
}
