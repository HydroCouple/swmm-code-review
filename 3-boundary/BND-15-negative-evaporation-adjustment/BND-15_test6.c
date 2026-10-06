/*
 * BND-15 for 6.0.0: a negative [ADJUSTMENTS] EVAPORATION value larger than the
 * base rate makes the evaporation rate negative, and negative evaporation adds
 * water.
 *
 * Same deck and check as BND-15_test.c. The adjusted rate should be
 * max(0, 0.05 - 0.10) = 0 in/day: the subcatchment's evaporation rate never
 * goes below 0, the runoff balance books no evaporation and the depression
 * storage holds exactly 0.2 in at the end.
 *
 * swmm_subcatch_get_evap() returns in/day; swmm_get_runoff_total() returns
 * ft3, converted here to inches over the 10-acre subcatchment. Tolerances as
 * in the legacy test: evaporation loss >= -0.001 in, final storage <= 0.201 in,
 * lowest rate >= 0.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    SWMM_Engine e = swmm_engine_create();
    const double ft3PerInch = 10.0 * 43560.0 / 12.0;     /* 10 acres */
    double t = 0.0, rate = 0.0, minRate = 1.0e10, evapLoss = 0.0, finalStore = 0.0;
    int rc;

    rc = swmm_engine_open(e, "BND-15_negative-adjustment.inp", "BND-15_6.rpt", "BND-15_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_subcatch_get_evap(e, 0, &rate);           /* in/day */
        if (rate < minRate) minRate = rate;
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

    printf("Base rate 0.05 in/day, adjustment -0.10 in/day\n");
    printf("Lowest subcatchment evaporation rate   %8.4f in/day  (expected >= 0)\n", minRate);
    printf("Runoff balance: evaporation loss       %8.3f in      (expected 0.000)\n", evapLoss);
    printf("Runoff balance: final storage          %8.3f in      (expected 0.200)\n", finalStore);

    if (minRate < 0.0 || evapLoss < -0.001 || finalStore > 0.201)
    {
        printf("FAIL: the adjusted evaporation rate is negative and adds water: "
               "rate %.4f in/day, evaporation loss %.3f in, final storage %.3f in "
               "with 0.2 in of depression storage\n", minRate, evapLoss, finalStore);
        return 1;
    }
    printf("PASS: the adjusted evaporation rate stops at zero and no water is created\n");
    return 0;
}
