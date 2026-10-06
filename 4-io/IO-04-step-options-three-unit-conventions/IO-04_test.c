/*
 * IO-04: [OPTIONS] times and steps written as a bare number.
 *
 * The legacy reader takes a bare number in START_TIME, END_TIME,
 * REPORT_START_TIME, WET_STEP, DRY_STEP, REPORT_STEP and RULE_STEP as decimal
 * hours (datetime_strToTime(): "accepts time as hr:min:sec or as decimal
 * hours"), and in ROUTING_STEP, LENGTHENING_STEP and MINIMUM_STEP as seconds.
 * IO-04_decimal-hours.inp uses both conventions:
 *     START_TIME 6.5   -> 06:30         END_TIME 12.5 -> 12:30 (a 6-hour run)
 *     REPORT_STEP 0.25 -> 900 s         ROUTING_STEP 30 -> 30 s
 *
 * Correct behaviour: the run starts on 01/01/2020 at 06:30 (day 43831 +
 * 6.5/24), lasts 6 hours (the last non-zero elapsed time is within one 30-s
 * routing step of 0.25 d; the legacy engine returns 0 for the final step)
 * and reports every 900 s. The legacy engines define this convention, so
 * they are expected to pass; the 6.0.0 test is the one that checks parity.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    const double startExp = 43831.0 + 6.5 / 24.0;
    double t = 0.0, tLast = 0.0, start = 0.0, rptStep = 0.0, routeStep = 0.0;
    int err, steps = 0, ok;

    err = swmm_open("IO-04_decimal-hours.inp", "IO-04.rpt", "IO-04.out");
    if (!err) err = swmm_start(0);
    if (!err)
    {
        start = swmm_getValue(swmm_STARTDATE, 0);
        rptStep = swmm_getValue(swmm_REPORTSTEP, 0);
        routeStep = swmm_getValue(swmm_ROUTESTEP, 0);
    }
    while (!err)
    {
        err = swmm_step(&t);
        if (!(t > 0.0)) break;
        tLast = t;
        if (++steps >= 20000) break;
    }
    swmm_end();
    swmm_close();

    printf("                       expected      read\n");
    printf("start date (days)  %12.6f  %12.6f   (01/01/2020 06:30)\n", startExp, start);
    printf("run length (h)     %12.4f  %12.4f\n", 6.0, tLast * 24.0);
    printf("report step (s)    %12.0f  %12.0f\n", 900.0, rptStep);
    printf("routing step (s)   %12.0f  %12.0f\n", 30.0, routeStep);
    printf("error code %d, %d steps\n", err, steps);

    ok = !err && fabs(start - startExp) < 0.5 / 86400.0
         && fabs(tLast - 0.25) <= 31.0 / 86400.0
         && rptStep == 900.0 && routeStep == 30.0;
    if (!ok)
    {
        printf("FAIL: decimal-hour START_TIME/END_TIME/REPORT_STEP not read as hours "
               "(start %.6f, run %.4f h, report step %.0f s)\n", start, tLast * 24.0, rptStep);
        return 1;
    }
    printf("PASS: START_TIME 6.5, END_TIME 12.5 and REPORT_STEP 0.25 are read as decimal "
           "hours, ROUTING_STEP 30 as seconds\n");
    return 0;
}
