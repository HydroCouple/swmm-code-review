/*
 * NUM-58 for 6.0.0: 5.2.4 drops the pollutant load of subcatchment runoff
 * that is below the MIN_RUNOFF cutoff (0.001 in/hr).
 *
 * 6.0.0 computes the runoff concentration from the pre-LID outflow volume
 * without the cutoff, like the vendored 5.3.0, so this test is expected to
 * pass unpatched.
 *
 * Deck: 1000 ac impervious, no depression storage, drizzle of 0.0005 in/hr
 * for 10 h at 10 mg/L, no buildup, no evaporation; the runoff stays below
 * the cutoff.
 *
 * Correct behaviour, as in NUM-58_test.c: the only water reaching J1 is
 * rain at 10 mg/L, so J1's concentration is 10 mg/L whenever it has inflow.
 * The test reads J1's inflow and P1 after every routing step and requires
 * 10 mg/L within 1 % (5.2.4 gives 0).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

int main(void)
{
    double t = 0.0, cMin = 1e30, cMax = -1e30, qMax = 0.0;
    int rc, j1 = -1, nsteps = 0, nwet = 0;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "NUM-58_drizzle.inp", "NUM-58_6.rpt", "NUM-58_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    if (!rc) j1 = swmm_node_index(e, "J1");
    while (!rc)
    {
        double q = 0.0, c = 0.0;
        rc = swmm_engine_step(e, &t);
        if (rc || t <= 0.0) break;
        nsteps++;
        swmm_node_get_inflow(e, j1, &q);
        swmm_node_get_quality(e, j1, 0, &c);
        if (q > 1.0e-6)
        {
            nwet++;
            if (q > qMax) qMax = q;
            if (c < cMin) cMin = c;
            if (c > cMax) cMax = c;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    printf("Routing steps with inflow at J1 .......... %d of %d\n", nwet, nsteps);
    printf("Largest inflow at J1 (cfs) ............... %.5f\n", qMax);
    printf("P1 at J1 while it has inflow (mg/L) ...... %.3f to %.3f (rain: 10.000)\n", cMin, cMax);
    if (nwet == 0 || !(fabs(cMin - 10.0) < 0.1) || !(fabs(cMax - 10.0) < 0.1))
    {
        printf("FAIL: runoff below the MIN_RUNOFF cutoff reaches J1 without its pollutant "
               "(%.3f to %.3f mg/L instead of 10)\n", cMin, cMax);
        return 1;
    }
    printf("PASS: runoff below the cutoff carries its 10 mg/L load to the network\n");
    return 0;
}
