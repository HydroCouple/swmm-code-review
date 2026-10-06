/*
 * CRASH-15: the 5.3.0 land-use buildup setters accept any number as the
 * time-series index of an EXTERNAL buildup function.
 *
 * For EXTERNAL buildup, coefficient 3 holds the index of the loading time
 * series (landuse.c stores c[2] = n when it reads the series name), and
 * landuse_getExternalBuildup() reads Tseries[(int)floor(coeff[2])] at every
 * runoff step. swmm_setValueExpanded(swmm_LANDUSE, swmm_LANDUSE_BUILDUP_COEFF3,
 * ...) accepts any value >= 0, and swmm_LANDUSE_BUILDUP_FUNC accepts any
 * function code, so a function can be made EXTERNAL with any coefficient 3.
 *
 * The deck has two time series (TSR = 0, TSB = 1). Each case opens it, makes
 * one of these edits before swmm_start(), and runs the whole day:
 *   1. TSS (EXTERNAL, series TSB): COEFF3 = 2, one past the last series
 *   2. TSS (EXTERNAL):             COEFF3 = 3e9, beyond the range of an int
 *   3. LEAD (POWER, COEFF3 0.5):   COEFF3 = 1000, then FUNC = 4 (EXTERNAL)
 * Case 0 sets COEFF3 = 1 (TSB itself), which must be accepted.
 * Correct behaviour: the setter rejects a time-series index that does not
 * exist (non-zero return code), as the input parser rejects an unknown series
 * name, and the run completes. Without the fix, case 1 reads past the end of
 * Tseries[] (AddressSanitizer stops the test), case 2 converts 3e9 to int
 * (undefined behaviour) and case 3 reads Tseries[1000].
 *
 * 5.2.4 has no land-use setters, so it is not affected.
 */
#include <stdio.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
/* Opens the deck, sets prop1 (and prop2, if >= 0) of the pollutant's buildup
 * function, runs to the end. Returns the number of setter calls that were
 * rejected; *err gets the run's error code. */
static int runCase(const char *pollut, int prop1, double v1, int prop2, double v2, int *err)
{
    double t = 0.0;
    int lu, p, rc1, rc2 = 0;

    *err = swmm_open("CRASH-15_buildup.inp", "CRASH-15.rpt", "CRASH-15.out");
    if (*err) { swmm_close(); return 0; }
    lu = swmm_getIndex(swmm_LANDUSE, "RES");
    p  = swmm_getIndex(swmm_POLLUTANT, pollut);
    rc1 = swmm_setValueExpanded(swmm_LANDUSE, prop1, lu, p, -1, v1);
    if (prop2 >= 0) rc2 = swmm_setValueExpanded(swmm_LANDUSE, prop2, lu, p, -1, v2);
    printf("  setter return codes: %d %s", rc1, prop2 >= 0 ? "" : "\n");
    if (prop2 >= 0) printf("%d\n", rc2);
    fflush(stdout);
    *err = swmm_start(0);
    while (!*err)
    {
        *err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    printf("  run finished, error code %d\n", *err);
    return (rc1 != 0) + (rc2 != 0);
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    int err, rejected, nbad = 0;

    printf("0. TSS (EXTERNAL): BUILDUP_COEFF3 = 1 (TSB, a valid index; must be accepted)\n");
    rejected = runCase("TSS", swmm_LANDUSE_BUILDUP_COEFF3, 1.0, -1, 0.0, &err);
    if (rejected || err) nbad++;

    printf("1. TSS (EXTERNAL): BUILDUP_COEFF3 = 2 (there are 2 time series)\n");
    rejected = runCase("TSS", swmm_LANDUSE_BUILDUP_COEFF3, 2.0, -1, 0.0, &err);
    if (rejected < 1 || err) nbad++;

    printf("2. TSS (EXTERNAL): BUILDUP_COEFF3 = 3e9\n");
    rejected = runCase("TSS", swmm_LANDUSE_BUILDUP_COEFF3, 3.0e9, -1, 0.0, &err);
    if (rejected < 1 || err) nbad++;

    printf("3. LEAD (POWER): BUILDUP_COEFF3 = 1000, then BUILDUP_FUNC = 4 (EXTERNAL)\n");
    rejected = runCase("LEAD", swmm_LANDUSE_BUILDUP_COEFF3, 1000.0,
                       swmm_LANDUSE_BUILDUP_FUNC, 4.0, &err);
    if (rejected < 1 || err) nbad++;

    if (nbad)
    {
        printf("FAIL: %d of 4 cases went wrong (an invalid time-series index accepted, or a valid one rejected)\n", nbad);
        return 1;
    }
    printf("PASS: the valid time-series index was accepted, every invalid one was rejected, and the runs completed\n");
    return 0;
#else
    printf("PASS: not affected, 5.2.4 has no land-use buildup setters\n");
    return 0;
#endif
}
