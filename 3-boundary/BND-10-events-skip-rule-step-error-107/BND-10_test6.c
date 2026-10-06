/*
 * BND-10 for 6.0.0: control rules are not evaluated between [EVENTS].
 *
 * 6.0.0 applies the rule-grid clamp to the between-events step, so it does
 * not stop with ERROR 107 like 5.2.4 and 5.3.0. But its routing step returns
 * as soon as it finds the current time outside every event, before it
 * evaluates the control rules (legacy evaluates them first and then skips
 * only the flow routing), so no rule acts between events at all.
 *
 * Correct behaviour, as in BND-10_test.c: the 6-hour run finishes without
 * error, and rule R1 (IF SIMULATION TIME > 1 THEN ORIFICE OR1 SETTING = 0.2)
 * acts at 01:00. The test allows the action to show up by 01:02 (one rule
 * step plus one routing step after 01:00); the bug sets it at 03:00, when
 * the event starts.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    double t = 0.0, tLast = 0.0, tAct = -1.0, setting = 0.0;
    int rc, or1 = -1;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "BND-10_events-rulestep.inp", "BND-10_6.rpt", "BND-10_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    if (!rc) or1 = swmm_link_index(e, "OR1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        tLast = t;
        swmm_link_get_control_setting(e, or1, &setting);
        if (tAct < 0.0 && fabs(setting - 0.2) < 1e-6) tAct = t;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    printf("Last step ended at        %8.4f h  (end is 6 h)\n", tLast * 24.0);
    printf("Error code                %8d\n", rc);
    if (tAct >= 0.0)
        printf("OR1 setting 0.2 from      %8.4f h  (rule should act at 1.0000 h)\n", tAct * 24.0);
    else
        printf("OR1 setting 0.2 from         never  (rule should act at 1.0000 h)\n");

    if (rc || tLast * 24.0 < 6.0 - 1e-6 || tAct < 0.0 || tAct * 86400.0 > 3600.0 + 120.0)
    {
        if (rc)
            printf("FAIL: the run stopped with error %d at %.4f h", rc, tLast * 24.0);
        else
            printf("FAIL: the run finished");
        if (tAct < 0.0)
            printf(" and rule R1 never acted (due at 1 h)\n");
        else
            printf(" and rule R1 acted at %.4f h instead of 1 h\n", tAct * 24.0);
        return 1;
    }
    printf("PASS: the run finished and rule R1 acted at 1 h, between events\n");
    return 0;
}
