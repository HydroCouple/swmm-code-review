/*
 * NUM-13: the seepage bottom area of FUNCTIONAL, CYLINDRICAL, CONICAL and
 * PYRAMIDAL storage units is left in user units (m2 used as ft2 under SI).
 *
 * Each deck holds the same physical storage unit three times: a vertical-
 * walled basin of 100 m2 (1076.39 ft2), 2 m (6.562 ft) of water, no inflow,
 * no evaporation, seeping into soil with Green-Ampt parameters Psi 100 mm,
 * Ksat 10 mm/hr, IMD 0.3, described as FUNCTIONAL (A0 = 100), TABULAR (a
 * constant-area curve) and CYLINDRICAL (11.2838 m diameter). NUM-13_si.inp
 * is in CMS, NUM-13_us.inp the same basin in CFS.
 *
 * Correct behaviour: the same basin over the same soil loses the same volume
 * whichever shape keyword and unit system describe it. The loss is the drop
 * in stored volume over the 6-hour run (nothing else enters or leaves).
 *
 * Tolerance: all six losses must agree within 1%. The US results and the SI
 * TABULAR result agree to 0.01%; the defect makes the SI FUNCTIONAL and
 * CYLINDRICAL units lose about 20% less.
 */
#include <stdio.h>
#include "swmm5.h"

#define M3_PER_FT3 0.0283168

static int run(const char *inp, const char *rpt, const char *out,
               double m3per, double loss[3])
{
    const char *ids[3] = {"SU1", "SU2", "SU3"};
    double t = 0.0, v0[3], v[3];
    int i, k[3], err;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    for (i = 0; i < 3 && !err; i++)
    {
        k[i] = swmm_getIndex(swmm_NODE, ids[i]);
        v0[i] = v[i] = swmm_getValue(swmm_NODE_VOLUME, k[i]);
    }
    while (!err)
    {
        err = swmm_step(&t);
        if (err) break;
        for (i = 0; i < 3; i++) v[i] = swmm_getValue(swmm_NODE_VOLUME, k[i]);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_report();
    swmm_close();
    for (i = 0; i < 3; i++) loss[i] = (v0[i] - v[i]) * m3per;
    return err;
}

int main(void)
{
    const char *shape[3] = {"FUNCTIONAL", "TABULAR", "CYLINDRICAL"};
    double si[3] = {0}, us[3] = {0}, lo = 1e30, hi = 0.0;
    int i, err;

    err = run("NUM-13_si.inp", "NUM-13_si.rpt", "NUM-13_si.out", 1.0, si);
    if (!err) err = run("NUM-13_us.inp", "NUM-13_us.rpt", "NUM-13_us.out",
                        M3_PER_FT3, us);
    if (err)
    {
        printf("FAIL: a run stopped with error %d\n", err);
        return 1;
    }

    printf("Seepage loss over 6 h (m3)\n");
    printf("Shape          SI (CMS)   US (CFS)\n");
    for (i = 0; i < 3; i++)
    {
        printf("%-12s  %9.3f  %9.3f\n", shape[i], si[i], us[i]);
        if (si[i] < lo) lo = si[i];
        if (si[i] > hi) hi = si[i];
        if (us[i] < lo) lo = us[i];
        if (us[i] > hi) hi = us[i];
    }
    if (hi > 1.01 * lo)
    {
        printf("FAIL: the same basin loses between %.3f and %.3f m3 "
               "(%.1f%% apart) depending on shape and units\n",
               lo, hi, 100.0 * (hi - lo) / hi);
        return 1;
    }
    printf("PASS: the same basin loses the same volume (%.3f m3) for every "
           "shape and unit system\n", lo);
    return 0;
}
