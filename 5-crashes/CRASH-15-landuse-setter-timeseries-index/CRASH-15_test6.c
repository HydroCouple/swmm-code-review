/*
 * CRASH-15 for 6.0.0: swmm_buildup_set() with an EXTERNAL function and a
 * time-series index that does not exist.
 *
 * In 6.0.0 coefficient 3 of an EXTERNAL buildup function is the index of the
 * loading time series (QualityHandler stores find_timeseries(name)), and the
 * runoff step does int ts_idx = static_cast<int>(bp.coeff[2]) and looks the
 * series up only if the index is in range. swmm_buildup_set() stores any
 * function code and coefficients.
 *
 * Same cases as the legacy test, made after swmm_engine_open() and before
 * swmm_engine_initialize(), each followed by a full run:
 *   1. TSS: EXTERNAL, c3 = 2 (there are 2 time series)
 *   2. TSS: EXTERNAL, c3 = 3e9 (beyond the range of an int)
 *   3. LEAD: POWER switched to EXTERNAL with c3 = 1000
 * Case 0 sets c3 = 1 (TSB itself), which must be accepted.
 * Correct behaviour: swmm_buildup_set() rejects a time-series index that does
 * not exist, as the input parser rejects an unknown series name, and the run
 * completes. Case 2 is undefined behaviour without the fix (float to int
 * conversion out of range, reported by UndefinedBehaviorSanitizer); cases 1
 * and 3 are accepted and the run silently has no external loading.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_quality.h"
#include "openswmm/engine/openswmm_pollutants.h"

static int runCase(const char *pollut, int func, double c1, double c2, double c3, int *err)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0;
    int lu, p, rc = 0;

    *err = swmm_engine_open(e, "CRASH-15_buildup.inp", "CRASH-15_6.rpt", "CRASH-15_6.out", NULL);
    if (!*err)
    {
        lu = swmm_landuse_index(e, "RES");
        p  = swmm_pollutant_index(e, pollut);
        rc = swmm_buildup_set(e, lu, p, func, c1, c2, c3, 0);
        printf("  swmm_buildup_set returned %d\n", rc);
        fflush(stdout);
        *err = swmm_engine_initialize(e);
    }
    if (!*err) *err = swmm_engine_start(e, 0);
    while (!*err)
    {
        *err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!*err) *err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    printf("  run finished, error code %d\n", *err);
    return rc != 0;
}

int main(void)
{
    int err, nbad = 0;

    printf("0. TSS: EXTERNAL with c3 = 1 (TSB, a valid index; must be accepted)\n");
    if (runCase("TSS", 4, 50.0, 1.0, 1.0, &err) || err) nbad++;

    printf("1. TSS: EXTERNAL with c3 = 2 (there are 2 time series)\n");
    if (!runCase("TSS", 4, 50.0, 1.0, 2.0, &err) || err) nbad++;

    printf("2. TSS: EXTERNAL with c3 = 3e9\n");
    if (!runCase("TSS", 4, 50.0, 1.0, 3.0e9, &err) || err) nbad++;

    printf("3. LEAD: POWER changed to EXTERNAL with c3 = 1000\n");
    if (!runCase("LEAD", 4, 5.0, 0.5, 1000.0, &err) || err) nbad++;

    if (nbad)
    {
        printf("FAIL: %d of 4 cases went wrong (an invalid time-series index accepted, or a valid one rejected)\n", nbad);
        return 1;
    }
    printf("PASS: the valid time-series index was accepted, every invalid one was rejected, and the runs completed\n");
    return 0;
}
