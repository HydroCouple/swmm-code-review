/*
 * BND-12: TIMEOPEN / TIMECLOSED are counted from midnight of the start date,
 * not from the start of the run.
 *
 * The run starts at 06:00. Pump P1 starts ON, pump P2 starts OFF.
 *     RULE TOPEN:   IF PUMP P1 TIMEOPEN >= 4   THEN PUMP P1 STATUS = OFF
 *     RULE TCLOSED: IF PUMP P2 TIMECLOSED >= 2 THEN PUMP P2 STATUS = ON
 *
 * Pump P3 starts ON and no rule acts on it.
 *
 * Correct behaviour: no pump has changed state since the run started, so P1
 * has been open for 4 h at 10:00 and P2 closed for 2 h at 08:00. The test
 * requires P1 to switch off and P2 on within one minute (two routing steps)
 * of those times, and P3's TIMEOPEN read through swmm_getValue() after the
 * first 30 s step to be under 0.02 h. With the bug both clocks start at
 * 00:00, both rules fire on the first step, and P3's TIMEOPEN reads 6.008 h.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, tOff = -1.0, tOn = -1.0, timeOpen1 = -1.0;
    int err, P1, P2, P3, steps = 0, ok;

    err = swmm_open("BND-12_start-0600.inp", "BND-12.rpt", "BND-12.out");
    if (!err) err = swmm_start(0);
    P1 = swmm_getIndex(swmm_LINK, "P1");
    P2 = swmm_getIndex(swmm_LINK, "P2");
    P3 = swmm_getIndex(swmm_LINK, "P3");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        if (++steps == 1) timeOpen1 = swmm_getValue(swmm_LINK_TIMEOPEN, P3);
        if (tOff < 0.0 && swmm_getValue(swmm_LINK_SETTING, P1) == 0.0) tOff = t * 24.0;
        if (tOn  < 0.0 && swmm_getValue(swmm_LINK_SETTING, P2) > 0.0)  tOn  = t * 24.0;
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("Run starts at 06:00 (hours below are since the start)\n");
    printf("  P3 TIMEOPEN after the first 30 s step: %.3f h (expected 0.008)\n", timeOpen1);
    printf("  P1 switched off by TOPEN at   %6.3f h (expected 4.000, i.e. 10:00)\n", tOff);
    printf("  P2 switched on by TCLOSED at  %6.3f h (expected 2.000, i.e. 08:00)\n", tOn);

    ok = timeOpen1 >= 0.0 && timeOpen1 < 0.02
      && fabs(tOff - 4.0) <= 1.0 / 60.0 + 1e-9
      && fabs(tOn - 2.0) <= 1.0 / 60.0 + 1e-9;
    if (!ok)
    {
        printf("FAIL: TIMEOPEN/TIMECLOSED count from midnight: P1 off after %.3f h, P2 on after "
               "%.3f h, P3 TIMEOPEN %.3f h at the first step\n", tOff, tOn, timeOpen1);
        return 1;
    }
    printf("PASS: TIMEOPEN and TIMECLOSED count from the start of the run\n");
    return 0;
}
