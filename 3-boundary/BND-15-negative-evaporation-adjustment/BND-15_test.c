/*
 * BND-15: a negative [ADJUSTMENTS] EVAPORATION value larger than the base rate
 * makes the evaporation rate negative, and negative evaporation adds water.
 *
 * The deck has a 10-acre impervious subcatchment with 0.2 in of depression
 * storage, a MONTHLY evaporation rate of 0.05 in/day and an adjustment of
 * -0.10 in/day (a climate-change scenario that lowers evaporation). 0.5 in of
 * rain falls in the first hour of a 2-day run.
 *
 * Correct behaviour: evaporation cannot be negative. The adjusted rate is
 * max(0, 0.05 - 0.10) = 0, so no water evaporates, the depression storage
 * holds exactly 0.2 in at the end and the subcatchment's evaporation rate is
 * never below 0.
 *
 * The buggy engine uses -0.05 in/day: the report shows an evaporation loss of
 * -0.100 in and a final storage above 0.2 in. Tolerances: the evaporation loss
 * must be >= -0.001 in and the final storage <= 0.201 in (the report prints 3
 * decimals); the minimum evaporation rate must be >= 0.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* first value in inches of a line of the runoff continuity table */
static double rptValue(const char *rpt, const char *label)
{
    char line[256];
    double v1, v2 = -999.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -999.0;
    while (fgets(line, sizeof(line), f))
    {
        char *p = strstr(line, label);
        if (p)
        {
            p += strlen(label);                    /* skip the dot leader */
            while (*p == ' ' || *p == '.') p++;
            if (sscanf(p, "%lf %lf", &v1, &v2) == 2) break;
        }
    }
    fclose(f);
    return v2;
}

int main(void)
{
    double elapsed = 0.0, rate, minRate = 1.0e10, evapLoss, finalStore;
    int err;

    err = swmm_open("BND-15_negative-adjustment.inp", "BND-15.rpt", "BND-15.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        rate = swmm_getValue(swmm_SUBCATCH_EVAP, 0);    /* in/day */
        if (rate < minRate) minRate = rate;
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    evapLoss = rptValue("BND-15.rpt", "Evaporation Loss");
    finalStore = rptValue("BND-15.rpt", "Final Storage");
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
