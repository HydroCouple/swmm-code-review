/*
 * API-06: 5.3.0's swmm_SUBCATCH_EXTERNAL_POLLUTANT_BUILDUP.
 *
 * A value of 1.0 is set on S1 (10 ac, one land use, no buildup function of
 * its own, no rain) right after swmm_start, before the first swmm_step, and
 * the day is run with DRY_STEP 1 h and with DRY_STEP 5 min.
 *
 * Expected:
 *  1. Reading the property back gives the value that was set (1.0).
 *  2. The buildup that results does not depend on the runoff time step. The
 *     property is an external buildup per unit area for the time it is set
 *     (the patch defines it as lb/ac per day; a one-off addition of 1 lb/ac
 *     would give the same number after one day), so after the 24 h run S1
 *     carries 1.0 lb/ac (10 lb), read with swmm_SUBCATCH_POLLUTANT_BUILDUP
 *     (correct for a single land use, see API-07). Checked within 5%.
 * The defect adds the full value at every runoff step: 24 lb/ac with 1 h
 * steps and 288 lb/ac with 5 min steps.
 *
 * 5.2.4 has no such property and is not affected.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *readBack, double *buildup)
{
    double elapsed = 0.0;
    int err, s1;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    s1 = swmm_getIndex(swmm_SUBCATCH, "S1");
    if (!err) err = swmm_setValueExpanded(swmm_SUBCATCH,
                    swmm_SUBCATCH_EXTERNAL_POLLUTANT_BUILDUP, s1, 0, 0, 1.0);
    *readBack = swmm_getValueExpanded(swmm_SUBCATCH,
                    swmm_SUBCATCH_EXTERNAL_POLLUTANT_BUILDUP, s1, 0, 0);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        *buildup = swmm_getValueExpanded(swmm_SUBCATCH,
                    swmm_SUBCATCH_POLLUTANT_BUILDUP, s1, 0, 0);
    }
    swmm_end();
    swmm_report();
    swmm_close();
    return err;
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    double back1 = 0, b1 = 0, back5 = 0, b5 = 0;
    int err1, err5, bad = 0;

    err1 = runDeck("API-06_dry-step-1h.inp", "API-06_1h.rpt", "API-06_1h.out", &back1, &b1);
    err5 = runDeck("API-06_dry-step-5min.inp", "API-06_5min.rpt", "API-06_5min.out", &back5, &b5);

    printf("EXTERNAL_POLLUTANT_BUILDUP(S1, P1) set to 1.0 at the start of a dry day\n");
    printf("%-14s %10s %22s %16s\n", "DRY_STEP", "read back", "buildup after 24 h", "expected");
    printf("%-14s %10.4f %15.4f lb/ac %16s\n", "1 h", back1, b1, "1.0 lb/ac");
    printf("%-14s %10.4f %15.4f lb/ac %16s\n", "5 min", back5, b5, "1.0 lb/ac");
    if (err1 || err5) { printf("FAIL: run error %d / %d\n", err1, err5); return 1; }

    if (fabs(back1 - 1.0) > 1e-9 || fabs(back5 - 1.0) > 1e-9)
    {
        printf("-> the getter does not return the value set\n"); bad++;
    }
    if (fabs(b1 - 1.0) > 0.05 || fabs(b5 - 1.0) > 0.05)
    {
        printf("-> the buildup added depends on the runoff time step\n"); bad++;
    }
    if (bad)
    {
        printf("FAIL: external buildup reads back as %.4g and adds %.4g lb/ac (1 h steps) "
               "vs %.4g lb/ac (5 min steps) for one value of 1.0\n", back1, b1, b5);
        return 1;
    }
    printf("PASS: the external buildup reads back as set and adds 1.0 lb/ac per day "
           "whatever the runoff time step\n");
    return 0;
#else
    printf("5.2.4 has no swmm_setValueExpanded and no external buildup property\n");
    printf("PASS: not affected (the API does not exist in 5.2.4)\n");
    return 0;
#endif
}
