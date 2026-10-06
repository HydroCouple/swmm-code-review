/*
 * BND-10: with [EVENTS] and a RULE_STEP that is not a multiple of
 * ROUTING_STEP, control rules stop being evaluated between events and the run
 * aborts with ERROR 107 when the next event starts.
 *
 * Between events routing_getRoutingStep() returns the fixed routing step
 * before it reaches the clamp that makes each step end on the next rule
 * time. With ROUTING_STEP 45 s and RULE_STEP 60 s the clock goes 0, 45, 90 s
 * and never lands on 60 s, so NewRuleTime stays at 0, rules are not evaluated
 * again, and when the clamp is finally applied (the step that would cross the
 * event start) it gives a negative step: ERROR 107.
 *
 * Correct behaviour: the 6-hour run finishes without error, and rule R1
 * (IF SIMULATION TIME > 1 THEN ORIFICE OR1 SETTING = 0.2) acts at 01:00:
 * control rules are evaluated at every rule step whether or not flow is
 * being routed. The test allows the action to show up by 01:02 (one rule
 * step plus one routing step after 01:00); the bug never sets it.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, tLast = 0.0, tAct = -1.0, setting;
    int err, or1;

    err = swmm_open("BND-10_events-rulestep.inp", "BND-10.rpt", "BND-10.out");
    if (!err) err = swmm_start(1);
    or1 = swmm_getIndex(swmm_LINK, "OR1");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        tLast = t;
        setting = swmm_getValue(swmm_LINK_SETTING, or1);
        if (tAct < 0.0 && fabs(setting - 0.2) < 1e-6) tAct = t;
    }
    swmm_end();
    swmm_close();

    printf("Last step ended at        %8.4f h  (swmm_step returns 0 at the 6 h end)\n",
           tLast * 24.0);
    printf("Error code                %8d\n", err);
    if (tAct >= 0.0)
        printf("OR1 setting 0.2 from      %8.4f h  (rule should act at 1.0000 h)\n", tAct * 24.0);
    else
        printf("OR1 setting 0.2 from         never  (rule should act at 1.0000 h)\n");

    if (err || tAct < 0.0 || tAct * 86400.0 > 3600.0 + 120.0)
    {
        if (err)
            printf("FAIL: the run stopped with error %d at %.4f h", err, tLast * 24.0);
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
