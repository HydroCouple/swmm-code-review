/*
 * NUM-40 for 6.0.0: the monthly [ADJUSTMENTS] RAINFALL factor must be the one
 * of the month the rain falls in.
 *
 * Same deck and check as NUM-40_test.c: factors of 2.0 for January and 3.0
 * for February, two 1-hour storms of 1 in (31 Jan 06:00 and 1 Feb 06:00).
 * The storms must fall at 2.0 and 3.0 in/hr and the report's Total
 * Precipitation must be 5.000 in. swmm_subcatch_get_rainfall returns the rate
 * in project units (in/hr here). A tolerance of 0.01 separates the right
 * values from the legacy ones (1.0 and 2.0 in/hr, 3.000 in) by a factor of 100.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"

#define RPT "NUM-40_6.rpt"

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
    double t = 0.0, r, peak[2] = {0.0, 0.0}, total;
    const double expect[2] = {2.0, 3.0};
    const char *day[2] = {"31 Jan (factor 2.0)", " 1 Feb (factor 3.0)"};
    int rc, d, ok = 1;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "NUM-40_two-months.inp", RPT, "NUM-40_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        r = 0.0;
        swmm_subcatch_get_rainfall(e, 0, &r);
        d = (t < 1.0) ? 0 : 1;
        if (r > peak[d]) peak[d] = r;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
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
