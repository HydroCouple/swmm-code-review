/*
 * CRASH-21 for 6.0.0: a rain gage time series with a single record.
 *
 * Same deck and check as CRASH-21_test.c: one record, 1.0 in/hr at 0:00 on a
 * gage with a 1-hour recording interval, run for that hour. A one-point
 * series has no spacing between records, so the run must validate without
 * undefined behaviour, without WARNING 09 or ERROR 159 about the interval,
 * and give 1.000 in of precipitation. The 0.01 in tolerance only guards
 * against a broken run.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

#define RPT "CRASH-21_6.rpt"

int main(void)
{
    char line[256];
    double precip = -1.0, vol;
    int err, warn09 = 0, err159 = 0;
    FILE *f;

    /* swmm_engine_run opens and validates the project (the gage interval
       checks), runs it and writes the report */
    err = swmm_engine_run("CRASH-21_one-record.inp", RPT, "CRASH-21_6.out", NULL);

    f = fopen(RPT, "r");
    while (f && fgets(line, sizeof(line), f))
    {
        char *p = strstr(line, "Total Precipitation ......");
        if (p && precip < 0.0) sscanf(p + 26, "%lf %lf", &vol, &precip);
        if (strstr(line, "WARNING 09")) warn09++;
        if (strstr(line, "ERROR 159")) err159++;
    }
    if (f) fclose(f);

    printf("run error code            %d\n", err);
    printf("WARNING 09 / ERROR 159    %d / %d   (expected 0 / 0)\n", warn09, err159);
    printf("Total Precipitation (in)  %.3f   (expected 1.000)\n", precip);

    if (err || warn09 || err159 || fabs(precip - 1.0) > 0.01)
    {
        printf("FAIL: the one-record series gave error %d, %d WARNING 09, %d ERROR 159, "
               "%.3f in of precipitation\n", err, warn09, err159, precip);
        return 1;
    }
    printf("PASS: the one-record series validates without interval messages and "
           "gives 1.000 in\n");
    return 0;
}
