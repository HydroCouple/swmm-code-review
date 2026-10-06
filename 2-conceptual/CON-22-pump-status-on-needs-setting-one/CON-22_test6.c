/*
 * CON-22 for 6.0.0: the premise "PUMP x STATUS = ON" is false for a pump
 * running at any speed setting other than 1.
 *
 * Same deck and check as CON-22_test.c. ControlEngine::getVariableValue()
 * returns the raw link setting for LINK_STATUS, and the rule's ON parses to 1.
 *
 * Correct behaviour: P1 is ON whenever its setting is above 0, so the
 * interlock rule RON keeps orifice ORON open while P1 runs at full speed
 * (0:15) and at half speed (1:00), and closes it once P1 is OFF (1:45):
 * ORON = 1, 1, 0.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    double t = 0.0, sample[3] = {0.25, 1.0, 1.75};      /* hours */
    double pSet[3], pFlow[3], orSet[3], expOr[3] = {1, 1, 0};
    int rc, P = -1, OR = -1, k = 0, i, ok = 1;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "CON-22_pump-half-speed.inp", "CON-22_6.rpt", "CON-22_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    if (!rc) { P = swmm_link_index(e, "P1"); OR = swmm_link_index(e, "ORON"); }
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        if (k < 3 && t * 24.0 >= sample[k] - 1e-9)
        {
            swmm_link_get_control_setting(e, P, &pSet[k]);
            swmm_link_get_flow(e, P, &pFlow[k]);
            swmm_link_get_control_setting(e, OR, &orSet[k]);
            k++;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc || k < 3)
    {
        printf("FAIL: the run stopped with error %d after %d samples\n", rc, k);
        return 1;
    }

    printf("  time   P1 setting   P1 flow (cfs)   ORON setting (expected)\n");
    for (i = 0; i < 3; i++)
    {
        printf("  %4.2f h   %6.2f        %6.3f          %4.1f  (%3.1f)\n",
               sample[i], pSet[i], pFlow[i], orSet[i], expOr[i]);
        if (fabs(orSet[i] - expOr[i]) > 1e-6) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: 'PUMP P1 STATUS = ON' is false while P1 runs at setting %.2f "
               "and pumps %.3f cfs (ORON = %.1f at 1:00)\n", pSet[1], pFlow[1], orSet[1]);
        return 1;
    }
    printf("PASS: 'PUMP P1 STATUS = ON' holds at full and half speed and not when P1 is off\n");
    return 0;
}
