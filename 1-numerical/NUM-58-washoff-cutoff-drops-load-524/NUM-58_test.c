/*
 * NUM-58: 5.2.4 drops the pollutant load of subcatchment runoff that is
 * below the MIN_RUNOFF cutoff (0.001 in/hr).
 *
 * surfqual_getWashoff() in 5.2.4 sets hasOutflow = (vOut2 > MIN_RUNOFF *
 * area * tStep) and computes the runoff concentration only if hasOutflow.
 * By then the washoff and ponded loads have already been taken off the
 * surface, and the runoff water itself is still routed. So below the
 * cutoff the water reaches the network with concentration 0 and the load
 * is booked nowhere. The vendored 5.3.0 computes the concentration whenever
 * there is runoff (fork commit e8a8d107, #90).
 *
 * Deck: 1000 ac impervious, no depression storage, drizzle of 0.0005 in/hr
 * for 10 h at 10 mg/L, no buildup, no evaporation. The runoff (about
 * 0.0025 cfs) stays below the cutoff all the time.
 *
 * Correct behaviour: the only water reaching J1 is rain at 10 mg/L, and
 * nothing adds or removes pollutant on the way, so J1's concentration is
 * 10 mg/L whenever it has inflow. The test reads J1's inflow and P1 for
 * every reporting period of the .out file and requires 10 mg/L within 1 %
 * (5.2.4 gives 0).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

int main(void)
{
    double t = 0.0, cMin = 1e30, cMax = -1e30, qMax = 0.0;
    int rc, j1 = -1, per, nper = 0, nwet = 0;
    SMO_Handle h = NULL;

    rc = swmm_open("NUM-58_drizzle.inp", "NUM-58.rpt", "NUM-58.out");
    if (!rc) rc = swmm_start(1);
    if (!rc) j1 = swmm_getIndex(swmm_NODE, "J1");
    while (!rc)
    {
        rc = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (rc || j1 < 0)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    SMO_init(&h);
    if (SMO_open(h, "NUM-58.out") || SMO_getTimes(h, SMO_numPeriods, &nper) || nper <= 0)
    {
        printf("FAIL: could not read NUM-58.out\n");
        return 1;
    }
    for (per = 0; per < nper; per++)
    {
        float *v = NULL;
        int len = 0;
        /* node results: depth, head, volume, lat. inflow, inflow, flooding, P1 */
        if (SMO_getNodeResult(h, per, j1, &v, &len) == 0 && len > 6 && v[4] > 1.0e-6)
        {
            nwet++;
            if (v[4] > qMax) qMax = v[4];
            if (v[6] < cMin) cMin = v[6];
            if (v[6] > cMax) cMax = v[6];
        }
        SMO_free((void **)&v);
    }
    SMO_close(&h);

    printf("Reporting periods with inflow at J1 ...... %d of %d\n", nwet, nper);
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
