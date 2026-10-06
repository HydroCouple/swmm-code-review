/*
 * NUM-14: storage_getLosses() returns the bare evaporation rate (ft/s) as a
 * flow (cfs) when the unit holds 1e-4 ft3 or less.
 *
 * The deck's storage unit has a constant 1000 ft2 surface, holds 5e-5 ft3
 * (below SWMM's FUDGE threshold of 1e-4 ft3), receives nothing and loses
 * water only to evaporation (1 in/day = 9.645e-7 ft/s, Fevap = 1).
 *
 * Correct behaviour: an evaporation loss is a flow, rate x surface area
 * (Reference Manual Vol. II, Eq. 7-14), capped at the volume stored: here
 * 1000 x 9.645e-7 = 9.6e-4 ft3/s, which empties the unit within any routing
 * step. Or, if the unit is treated as empty because it holds less than FUDGE
 * (the guard in the code, and SWMM 5.1.013's behaviour), nothing is lost.
 * Any other loss is wrong; a loss of 9.645e-7 ft3/s is the ft/s rate used as
 * a flow. The test checks the first three routing steps (0.5 s, then 10 s,
 * under dynamic wave).
 *
 * Tolerance: "nothing" is a loss below 1e-9 ft3 in a step, "emptied" leaves
 * less than 1% of the volume; the defect removes 2% to 19% per step.
 */
#include <stdio.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, tPrev = 0.0, vPrev, v, loss;
    int i, k, err, bad = 0;

    err = swmm_open("NUM-14_nearly-empty.inp", "NUM-14.rpt", "NUM-14.out");
    if (!err) err = swmm_start(1);
    if (err)
    {
        printf("FAIL: the run did not start (error %d)\n", err);
        return 1;
    }
    k = swmm_getIndex(swmm_NODE, "S1");
    vPrev = swmm_getValue(swmm_NODE_VOLUME, k);
    printf("Evaporation rate 9.645e-7 ft/s; rate x area = 9.645e-4 ft3/s\n");
    printf("Step  dt (s)  Volume before  Volume after   Loss (ft3)  Loss/dt (ft3/s)\n");
    for (i = 1; i <= 3 && !err; i++)
    {
        err = swmm_step(&t);
        v = swmm_getValue(swmm_NODE_VOLUME, k);
        loss = vPrev - v;
        printf("%4d  %6.2f  %13.4e  %12.4e  %11.4e  %15.4e\n", i,
               (t - tPrev) * 86400.0, vPrev, v, loss,
               loss / ((t - tPrev) * 86400.0));
        if (!(loss < 1.0e-9 || v < 0.01 * vPrev)) bad++;
        tPrev = t;
        vPrev = v;
    }
    while (!err && t > 0.0) err = swmm_step(&t);
    swmm_end();
    swmm_report();
    swmm_close();

    if (err || bad)
    {
        printf("FAIL: in %d of 3 steps the nearly empty unit lost the "
               "evaporation rate in ft/s as if it were a flow in ft3/s\n", bad);
        return 1;
    }
    printf("PASS: the evaporation loss of a nearly empty unit is either a flow "
           "over its area or zero\n");
    return 0;
}
