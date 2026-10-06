/*
 * BND-16: evaporation from a time series, run starting between two entries.
 *
 * The evaporation series has Dec 1 0.05, Jan 1 0.10, Feb 1 0.20 and Mar 1
 * 0.30 in/day. The run goes from Jan 15 to Feb 10. 6 in of rain in the first
 * hour fill a 6 in depression storage on a 10-acre impervious subcatchment, so
 * water is always available to evaporate.
 *
 * Correct behaviour: SWMM applies each series value from its own date to the
 * next entry's date (a step function, see setEvap). So the rate is 0.10 in/day
 * from Jan 15 to Feb 1 and 0.20 in/day from Feb 1 to Feb 10:
 *     evaporation loss = 17 d x 0.10 + 9 d x 0.20 = 3.50 in.
 *
 * The buggy engine starts with the Feb 1 entry's rate, 0.20 in/day, and the
 * loss is about 26 d x 0.20 = 5.20 in. Tolerances: the rate before Feb 1 must
 * be within 0.001 in/day of 0.10, the rate after Feb 1 within 0.001 of 0.20 and
 * the reported loss within 0.05 in of 3.50 (the bug is off by 1.70 in).
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* value in inches (second column) of a line of the runoff continuity table */
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
    const double feb1 = 17.0;                /* days from Jan 15 to Feb 1 */
    double elapsed = 0.0, rate;
    double janMin = 1e10, janMax = -1e10, febMin = 1e10, febMax = -1e10;
    double evapLoss, finalStore;
    int err;

    err = swmm_open("BND-16_mid-series-start.inp", "BND-16.rpt", "BND-16.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        rate = swmm_getValue(swmm_SUBCATCH_EVAP, 0);    /* in/day */
        /* skip the first 2 h (the surface is dry when the first step starts,
           so nothing evaporates) and the hour around Feb 1 00:00, where a
           runoff step may straddle the change */
        if (elapsed < 2.0 / 24.0) continue;
        if (elapsed < feb1 - 0.05)
        {
            if (rate < janMin) janMin = rate;
            if (rate > janMax) janMax = rate;
        }
        else if (elapsed > feb1 + 0.05)
        {
            if (rate < febMin) febMin = rate;
            if (rate > febMax) febMax = rate;
        }
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    evapLoss = rptValue("BND-16.rpt", "Evaporation Loss");
    finalStore = rptValue("BND-16.rpt", "Final Storage");
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
