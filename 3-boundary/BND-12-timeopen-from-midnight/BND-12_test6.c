/*
 * BND-12 for 6.0.0: TIMEOPEN / TIMECLOSED are counted from midnight of the
 * start date, not from the start of the run.
 *
 * Same deck and rule checks as BND-12_test.c (6.0.0 has no API getter for a
 * link's time open). SWMMEngine seeds every link's time_last_set with
 * floor(start_date), copying legacy link_initState().
 *
 * Correct behaviour: in a run that starts at 06:00, rule TOPEN (P1 TIMEOPEN
 * >= 4) switches P1 off at 10:00 and rule TCLOSED (P2 TIMECLOSED >= 2)
 * switches P2 on at 08:00, each to within one minute. With the bug both act
 * on the first step.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    double t = 0.0, tOff = -1.0, tOn = -1.0, s1 = 1.0, s2 = 0.0;
    int rc, P1 = -1, P2 = -1;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "BND-12_start-0600.inp", "BND-12_6.rpt", "BND-12_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    if (!rc) { P1 = swmm_link_index(e, "P1"); P2 = swmm_link_index(e, "P2"); }
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_link_get_control_setting(e, P1, &s1);
        swmm_link_get_control_setting(e, P2, &s2);
        if (tOff < 0.0 && s1 == 0.0) tOff = t * 24.0;
        if (tOn  < 0.0 && s2 > 0.0)  tOn  = t * 24.0;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    printf("Run starts at 06:00 (hours below are since the start)\n");
    printf("  P1 switched off by TOPEN at   %6.3f h (expected 4.000, i.e. 10:00)\n", tOff);
    printf("  P2 switched on by TCLOSED at  %6.3f h (expected 2.000, i.e. 08:00)\n", tOn);

    if (fabs(tOff - 4.0) > 1.0 / 60.0 + 1e-9 || fabs(tOn - 2.0) > 1.0 / 60.0 + 1e-9)
    {
        printf("FAIL: TIMEOPEN/TIMECLOSED count from midnight: P1 off after %.3f h, "
               "P2 on after %.3f h\n", tOff, tOn);
        return 1;
    }
    printf("PASS: TIMEOPEN and TIMECLOSED count from the start of the run\n");
    return 0;
}
