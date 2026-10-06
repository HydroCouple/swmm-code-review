/*
 * BND-16 for 6.0.0: evaporation from a time series, run starting between two
 * entries.
 *
 * Same deck and check as BND-16_test.c. The series (Dec 1 0.05, Jan 1 0.10,
 * Feb 1 0.20, Mar 1 0.30 in/day) is a step function, so the rate is 0.10
 * in/day from Jan 15 to Feb 1 and 0.20 in/day from Feb 1 to Feb 10, and the
 * evaporation loss is 17 d x 0.10 + 9 d x 0.20 = 3.50 in. The buggy engine
 * uses 0.20 in/day from the start (about 5.20 in).
 *
 * swmm_subcatch_get_evap() returns in/day; swmm_get_runoff_total() returns
 * ft3, converted here to inches over the 10-acre subcatchment;
 * swmm_engine_step() returns elapsed days. Tolerances as in the legacy test:
 * rates within 0.001 in/day, loss within 0.05 in of 3.50.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    SWMM_Engine e = swmm_engine_create();
    const double ft3PerInch = 10.0 * 43560.0 / 12.0;     /* 10 acres */
    const double feb1 = 17.0;                /* days from Jan 15 to Feb 1 */
    double t = 0.0, rate = 0.0;
    double janMin = 1e10, janMax = -1e10, febMin = 1e10, febMax = -1e10;
    double evapLoss = 0.0, finalStore = 0.0;
    int rc;

    rc = swmm_engine_open(e, "BND-16_mid-series-start.inp", "BND-16_6.rpt", "BND-16_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_subcatch_get_evap(e, 0, &rate);           /* in/day */
        /* skip the first 2 h (the surface is dry when the first step starts,
           so nothing evaporates) and the hour around Feb 1 00:00, where a
           runoff step may straddle the change */
        if (t < 2.0 / 24.0) continue;
        if (t < feb1 - 0.05)
        {
            if (rate < janMin) janMin = rate;
            if (rate > janMax) janMax = rate;
        }
        else if (t > feb1 + 0.05)
        {
            if (rate < febMin) febMin = rate;
            if (rate > febMax) febMax = rate;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc)
    {
        swmm_get_runoff_total(e, SWMM_RUNOFF_EVAP, &evapLoss);
        swmm_get_runoff_total(e, SWMM_RUNOFF_FINALSTORE, &finalStore);
        evapLoss /= ft3PerInch;
        finalStore /= ft3PerInch;
    }
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    printf("Series: Dec 1 0.05, Jan 1 0.10, Feb 1 0.20, Mar 1 0.30 in/day; run Jan 15 - Feb 10\n");
    printf("                                       observed        expected\n");
    printf("Rate Jan 15 2:00 - Feb 1 (min, max)  %6.4f %6.4f   0.1000 in/day\n", janMin, janMax);
    printf("Rate Feb 1 - Feb 10 (min, max)       %6.4f %6.4f   0.2000 in/day\n", febMin, febMax);
    printf("Runoff balance: evaporation          %8.3f        3.500 in\n", evapLoss);
    printf("Runoff balance: final storage        %8.3f        2.500 in\n", finalStore);

    if (janMin < 0.099 || janMax > 0.101 || febMin < 0.199 || febMax > 0.201 ||
        evapLoss < 3.45 || evapLoss > 3.55)
    {
        printf("FAIL: from Jan 15 to Feb 1 the series' Feb 1 rate is used instead of the "
               "Jan 1 rate: %.4f in/day instead of 0.1000, evaporation loss %.3f in "
               "instead of 3.500 in\n", janMax, evapLoss);
        return 1;
    }
    printf("PASS: the rate in effect at the start is the Jan 1 entry's 0.10 in/day and "
           "the Feb 1 entry applies from Feb 1; evaporation loss %.3f in\n", evapLoss);
    return 0;
}
