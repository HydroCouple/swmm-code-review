/*
 * CON-22: the premise "PUMP x STATUS = ON" is false for a pump running at any
 * speed setting other than 1.
 *
 * Pump P1 starts ON (setting 1). Rule R1 sets it to half speed (SETTING = 0.5)
 * after 0:30 and rule R3 switches it OFF after 1:30. Rule RON is an interlock:
 *     IF PUMP P1 STATUS = ON THEN ORIFICE ORON SETTING = 1
 *     ELSE ORIFICE ORON SETTING = 0
 *
 * Correct behaviour: a pump is ON whenever its setting is above 0 (it is
 * pumping; a SETTING action of 0.5 is a speed, not a third state), so ORON
 * stays open while P1 runs at full or half speed and closes once P1 is OFF.
 * The test samples at 0:15 (full speed), 1:00 (half speed) and 1:45 (off) and
 * expects ORON = 1, 1, 0. With the bug the STATUS premise compares the setting
 * with exactly 1, so ORON closes at 0:30 while P1 still pumps 1 cfs.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, sample[3] = {0.25, 1.0, 1.75};      /* hours */
    double pSet[3], pFlow[3], orSet[3], expOr[3] = {1, 1, 0};
    int err, P, OR, k = 0, i, ok = 1;

    err = swmm_open("CON-22_pump-half-speed.inp", "CON-22.rpt", "CON-22.out");
    if (!err) err = swmm_start(0);
    P  = swmm_getIndex(swmm_LINK, "P1");
    OR = swmm_getIndex(swmm_LINK, "ORON");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        if (k < 3 && t * 24.0 >= sample[k] - 1e-9)
        {
            pSet[k]  = swmm_getValue(swmm_LINK_SETTING, P);
            pFlow[k] = swmm_getValue(swmm_LINK_FLOW, P);
            orSet[k] = swmm_getValue(swmm_LINK_SETTING, OR);
            k++;
        }
    }
    swmm_end();
    swmm_close();
    if (err || k < 3)
    {
        printf("FAIL: the run stopped with error %d after %d samples\n", err, k);
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
