/*
 * NUM-40: the monthly [ADJUSTMENTS] RAINFALL factor is applied when a rain
 * record is read ahead, not in the month the rain falls.
 *
 * The deck has factors of 2.0 for January and 3.0 for February and two 1-hour
 * storms of 1 in each: 31 Jan 06:00 and 1 Feb 06:00. The documented rule is
 * that each month's rainfall is multiplied by that month's factor, so the
 * storms must fall at 2.0 in/hr and 3.0 in/hr, and the Total Precipitation in
 * the runoff continuity table must be 2 + 3 = 5.000 in.
 *
 * The test runs the deck step by step, records the largest rainfall rate on
 * S1 (swmm_SUBCATCH_RAINFALL, in/hr) during each day of the run, then reads
 * Total Precipitation from the report. The rates are exact multiples of
 * 1 in/hr; a tolerance of 0.01 separates the right values from the wrong ones
 * (1.0 and 2.0 in/hr, 3.000 in) by a factor of 100.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define RPT "NUM-40.rpt"

static double reportedPrecip(void)
{
    char line[256];
    double vol = -1.0, depth = -1.0;
    FILE *f = fopen(RPT, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof(line), f))
    {
        char *p = strstr(line, "Total Precipitation ......");
        if (p && sscanf(p + 26, "%lf %lf", &vol, &depth) == 2) break;
    }
    fclose(f);
    return depth;
}

int main(void)
{
    double elapsed = 0.0, r, peak[2] = {0.0, 0.0}, total;
    const double expect[2] = {2.0, 3.0};
    const char *day[2] = {"31 Jan (factor 2.0)", " 1 Feb (factor 3.0)"};
    int err, d, ok = 1;

    err = swmm_open("NUM-40_two-months.inp", RPT, "NUM-40.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        r = swmm_getValue(swmm_SUBCATCH_RAINFALL, 0);
        d = (elapsed < 1.0) ? 0 : 1;
        if (r > peak[d]) peak[d] = r;
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    total = reportedPrecip();

    printf("Storm                 rain on S1 (in/hr)   expected\n");
    for (d = 0; d < 2; d++)
    {
        printf("%s   %10.3f        %10.3f\n", day[d], peak[d], expect[d]);
        if (fabs(peak[d] - expect[d]) > 0.01) ok = 0;
    }
    printf("Total Precipitation (in)   %10.3f        %10.3f\n", total, 5.0);
    if (fabs(total - 5.0) > 0.01) ok = 0;

    if (!ok)
    {
        printf("FAIL: the storms fell at %.3f and %.3f in/hr (total %.3f in) "
               "instead of 2.000 and 3.000 in/hr (5.000 in)\n",
               peak[0], peak[1], total);
        return 1;
    }
    printf("PASS: each storm is scaled by the factor of the month it falls in "
           "(2.000 + 3.000 = 5.000 in)\n");
    return 0;
}
