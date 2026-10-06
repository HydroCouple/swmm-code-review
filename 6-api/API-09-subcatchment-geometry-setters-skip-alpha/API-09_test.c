/*
 * API-09: the 5.3.0 setters for subcatchment AREA, WIDTH and SLOPE do not
 * reach the runoff calculation.
 *
 * Overland flow from a subarea is q = alpha * (d - ds)^(5/3) with
 * alpha = 1.49 * width / area * sqrt(slope) / n. SWMM computes alpha once, in
 * subcatch_validate() during swmm_open(). swmm_setValueExpanded() accepts
 * AREA, WIDTH and SLOPE between swmm_open() and swmm_start() and stores the
 * new value, but nothing recomputes alpha.
 *
 * Correct behaviour: a value set through the API before swmm_start() gives
 * the same run as the same value written in the input file. For each of the
 * three properties the test opens API-09_base.inp (10 ac, width 500 ft,
 * slope 0.5 %, 100 % impervious, 2 in/hr for 10 min), sets the property, runs,
 * and compares the peak runoff of S1 with a run of the deck that has the
 * value in [SUBCATCHMENTS]. The same model and the same arithmetic are used
 * in both runs, so the peaks must agree to rounding; the test allows a
 * relative difference of 1e-6. The defect changes the peak by 50 to 80 %.
 *
 * 5.2.4 has no subcatchment geometry setters (swmm_setValue accepts only
 * gage rainfall, lateral inflow, outfall stage, link setting, report flags
 * and two time steps), so it is not affected.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
/* Runs inp; if prop >= 0, sets that subcatchment property of S1 to value
 * before swmm_start(). Returns the peak runoff of S1 (cfs) or -1 on error. */
static double peakRunoff(const char *inp, int prop, double value, int *rc,
                         double *readBack)
{
    double t = 0.0, q, qmax = 0.0;
    int s1, err;

    *rc = 0;
    err = swmm_open(inp, "API-09.rpt", "API-09.out");
    if (err) { swmm_close(); return -1.0; }
    s1 = swmm_getIndex(swmm_SUBCATCH, "S1");
    if (prop >= 0)
    {
        *rc = swmm_setValueExpanded(swmm_SUBCATCH, prop, s1, -1, -1, value);
        *readBack = swmm_getValueExpanded(swmm_SUBCATCH, prop, s1, -1, -1);
    }
    err = swmm_start(0);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        q = swmm_getValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_RUNOFF, s1, -1, -1);
        if (q > qmax) qmax = q;
    }
    swmm_end();
    swmm_close();
    return err ? -1.0 : qmax;
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    struct { const char *name; int prop; double value; const char *inp; } cases[] = {
        { "WIDTH 500 -> 5000 ft", swmm_SUBCATCH_WIDTH, 5000.0, "API-09_width5000.inp" },
        { "SLOPE 0.005 -> 0.05",  swmm_SUBCATCH_SLOPE, 0.05,   "API-09_slope5.inp"    },
        { "AREA 10 -> 20 ac",     swmm_SUBCATCH_AREA,  20.0,   "API-09_area20.inp"    },
    };
    int i, rc, dummy, nbad = 0;
    double q0, qApi, qInp, rel, readBack = 0.0, unused = 0.0, worst = 0.0;

    q0 = peakRunoff("API-09_base.inp", -1, 0.0, &dummy, &unused);
    printf("Peak runoff of S1 with API-09_base.inp unchanged: %.4f cfs\n\n", q0);
    printf("%-22s %4s %10s %14s %14s %9s\n", "Set before swmm_start", "rc",
           "read back", "peak via API", "peak via .inp", "diff");
    for (i = 0; i < 3; i++)
    {
        qApi = peakRunoff("API-09_base.inp", cases[i].prop, cases[i].value, &rc, &readBack);
        qInp = peakRunoff(cases[i].inp, -1, 0.0, &dummy, &unused);
        rel = (qInp > 0.0) ? fabs(qApi - qInp) / qInp : 1.0;
        printf("%-22s %4d %10g %14.4f %14.4f %8.2f%%\n", cases[i].name, rc,
               readBack, qApi, qInp, 100.0 * rel);
        if (rc != 0 || qApi < 0.0 || qInp < 0.0 || rel > 1.0e-6) nbad++;
        if (rel > worst) worst = rel;
    }
    if (nbad)
    {
        printf("FAIL: %d of 3 subcatchment geometry setters accepted the value "
               "but the runoff did not change as it does when the value is in the "
               "input file (largest peak difference %.1f %%)\n", nbad, 100.0 * worst);
        return 1;
    }
    printf("PASS: AREA, WIDTH and SLOPE set before swmm_start() give the same runoff "
           "as the same values in the input file\n");
    return 0;
#else
    printf("PASS: not affected, 5.2.4 has no subcatchment area, width or slope setter\n");
    return 0;
#endif
}
