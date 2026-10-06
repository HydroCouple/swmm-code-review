/*
 * CRASH-21: a rain gage time series with a single record makes 5.2.4's
 * gage_validate() convert 8.64e14 to int (undefined behaviour).
 *
 * The deck defines a design pulse as one record, 1.0 in/hr at 0:00 on a gage
 * with a 1-hour recording interval, and runs for that hour. A one-point
 * series has no spacing between records, so there is nothing to compare the
 * recording interval with: the run must validate without undefined behaviour
 * (UBSan reports "runtime error" otherwise), without WARNING 09 or ERROR 159
 * about the interval, and give 1.000 in of precipitation (1 in/hr for 1 h).
 * The precipitation tolerance of 0.01 in only guards against a broken run.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define RPT "CRASH-21.rpt"

int main(void)
{
    char line[256];
    double precip = -1.0, vol;
    int err, warn09 = 0, err159 = 0;
    FILE *f;

    /* swmm_open validates the project, which runs gage_validate() */
    err = swmm_open("CRASH-21_one-record.inp", RPT, "CRASH-21.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        double elapsed = 0.0;
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_report();
    swmm_close();

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
