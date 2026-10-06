/*
 * IO-32: grate type P_BAR-50x100 is read as P_BAR-50.
 *
 * The deck puts a 2 ft x 2 ft P_BAR-50x100 grate on grade on an 8% street
 * (Sx = 0.02, n = 0.016, no gutter depression, one side) at 6 and 12 cfs.
 *
 * Expected capture, HEC-22 (3rd ed.) section 4.4.4 for a conventional
 * gutter, computed here:
 *   spread       T  = (Q / ((0.56/n) SL^0.5 Sx^1.67))^0.375      Eq. 4-2
 *   velocity     V  = Q / (T^2 Sx / 2)
 *   frontal flow Eo = 1 - (1 - Wg/T)^2.67                        Eq. 4-16
 *   splash-over  Vo = 0.74 + 2.44 L - 0.27 L^2 + 0.02 L^3  (P_BAR-50x100,
 *                Chart 5B as fitted in inlet.c SplashCoeffs[1]; 4.70 ft/s
 *                for L = 2 ft. P_BAR-50 gives 8.16 ft/s)
 *   Rf = 1 - 0.09 (V - Vo) if V > Vo                             Eq. 4-18
 *   Rs = 1 / (1 + 0.15 V^1.8 / (Sx L^2.3))                       Eq. 4-19
 *   E  = Rf Eo + Rs (1 - Eo)                                     Eq. 4-21
 * Measured capture = drop in street flow across the inlet at the end of the
 * 3-hour run (steady state). Tolerance 2 percentage points; reading the
 * grate as P_BAR-50 overstates the capture by 7.5 and 10 points here.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#define NS 2
static double approachVelocity(double Q)
{
    const double n = 0.016, Sx = 0.02, SL = 0.08;
    double T = pow(Q / (0.56 / n * sqrt(SL) * pow(Sx, 1.67)), 0.375);
    return Q / (T * T * Sx / 2.0);
}

static double hec22Grate(double Q)
{
    const double n = 0.016, Sx = 0.02, SL = 0.08, Wg = 2.0, L = 2.0;
    double T, V, Eo, Vo, Rf = 1.0, Rs;
    T  = pow(Q / (0.56 / n * sqrt(SL) * pow(Sx, 1.67)), 0.375);
    V  = approachVelocity(Q);
    Eo = 1.0 - pow(1.0 - Wg / T, 2.67);
    Vo = 0.74 + 2.44 * L - 0.27 * L * L + 0.02 * L * L * L;
    if (V > Vo) Rf = 1.0 - 0.09 * (V - Vo);
    Rs = 1.0 / (1.0 + 0.15 * pow(V, 1.8) / (Sx * pow(L, 2.3)));
    return 100.0 * (Rf * Eo + Rs * (1.0 - Eo));
}

int main(void)
{
    const char *up[NS] = {"S1a", "S2a"}, *dn[NS] = {"S1b", "S2b"};
    double elapsed = 0.0, qu[NS] = {0}, qd[NS] = {0}, got, want, worst = 0;
    int iu[NS], id[NS], k, err, bad = 0;

    err = swmm_open("IO-32_pbar50x100-grate.inp", "IO-32.rpt", "IO-32.out");
    if (!err) err = swmm_start(1);
    if (err) { printf("FAIL: could not start the run (error %d)\n", err); return 1; }
    for (k = 0; k < NS; k++)
    {
        iu[k] = swmm_getIndex(swmm_LINK, up[k]);
        id[k] = swmm_getIndex(swmm_LINK, dn[k]);
    }
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        for (k = 0; k < NS; k++)
        {
            qu[k] = swmm_getValue(swmm_LINK_FLOW, iu[k]);
            qd[k] = swmm_getValue(swmm_LINK_FLOW, id[k]);
        }
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    printf("Splash-over velocity of a 2-ft grate: P_BAR-50x100 %.2f ft/s, P_BAR-50 %.2f ft/s\n",
           0.74 + 2.44 * 2 - 0.27 * 4 + 0.02 * 8, 2.22 + 4.03 * 2 - 0.65 * 4 + 0.06 * 8);
    printf("Street  Q (cfs)  V (ft/s)  captured (cfs)  capture %%  HEC-22 P_BAR-50x100 %%\n");
    for (k = 0; k < NS; k++)
    {
        got = 100.0 * (qu[k] - qd[k]) / qu[k];
        want = hec22Grate(qu[k]);
        printf("%-6s  %7.3f  %8.2f  %14.3f  %9.2f  %21.2f\n", up[k], qu[k], approachVelocity(qu[k]),
               qu[k] - qd[k], got, want);
        if (fabs(got - want) > 2.0) bad++;
        if (fabs(got - want) > worst) worst = fabs(got - want);
    }
    if (bad)
    {
        printf("FAIL: the P_BAR-50x100 grate captures up to %.2f points more than HEC-22 gives "
               "for that grate\n", worst);
        return 1;
    }
    printf("PASS: the P_BAR-50x100 grate follows HEC-22 for that grate (largest difference "
           "%.2f points)\n", worst);
    return 0;
}
