/*
 * NUM-14 for 6.0.0: the storage loss routine (Routing.cpp, as legacy
 * storage_getLosses()) returns the bare evaporation rate (ft/s) as a
 * flow (cfs) when the unit holds 1e-4 ft3 or less.
 *
 * Same deck and check as NUM-14_test.c: in each of the first three routing
 * steps the unit holding 5e-5 ft3 must lose either nothing (treated as empty)
 * or all of it (rate x 1000 ft2 = 9.6e-4 ft3/s, capped at the volume), not
 * the ft/s rate as ft3/s.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

int main(void)
{
    double t = 0.0, tPrev = 0.0, vPrev, v, loss;
    int i, k, err, bad = 0;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "NUM-14_nearly-empty.inp", "NUM-14_6.rpt",
                           "NUM-14_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (err)
    {
        printf("FAIL: the run did not start (error %d)\n", err);
        return 1;
    }
    k = swmm_node_index(e, "S1");
    swmm_node_get_volume(e, k, &vPrev);
    printf("Evaporation rate 9.645e-7 ft/s; rate x area = 9.645e-4 ft3/s\n");
    printf("Step  dt (s)  Volume before  Volume after   Loss (ft3)  Loss/dt (ft3/s)\n");
    for (i = 1; i <= 3 && !err; i++)
    {
        err = swmm_engine_step(e, &t);
        swmm_node_get_volume(e, k, &v);
        loss = vPrev - v;
        printf("%4d  %6.2f  %13.4e  %12.4e  %11.4e  %15.4e\n", i,
               (t - tPrev) * 86400.0, vPrev, v, loss,
               loss / ((t - tPrev) * 86400.0));
        if (!(loss < 1.0e-9 || v < 0.01 * vPrev)) bad++;
        tPrev = t;
        vPrev = v;
    }
    while (!err && t > 0) err = swmm_engine_step(e, &t);
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

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
