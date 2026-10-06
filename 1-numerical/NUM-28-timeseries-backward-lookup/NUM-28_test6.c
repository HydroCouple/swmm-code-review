/*
 * NUM-28 for 6.0.0: the same check as NUM-28_test.c through the 6.0.0 C API.
 *
 * TS1 and TS2 are each read by an EXT buildup function at the end of each
 * 18-minute runoff step and by a FLOW inflow (TS1 at J1, TS2 at J2) at the
 * start of each 1-minute routing step, so the first routing lookup after each
 * runoff step goes back in time.
 *
 * Correct behaviour: J1's (J2's) lateral inflow in each routing step is TS1
 * (TS2) interpolated at the step's start time (no rain, so no runoff), and the
 * cumulative external inflow volume is the integral of the two series,
 * 76,500 ft3. Tolerances as in NUM-28_test.c: 0.001 cfs per step (the lookup is
 * 1 ms after the step start, worth at most 7e-5 cfs) and 1 % on the volume
 * (the bug adds about 11 %).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

static const double T1[] = {0, 15, 30, 45, 60, 75, 90, 105};
static const double Q1[] = {0, 10, 20, 0, 0, 30, 5, 0};
static const double T2[] = {5, 10, 15, 20, 30};
static const double Q2[] = {0, 5, 10, 30, 0};
#define EXACT_VOLUME 76500.0   /* ft3 */

static double series(const double *T, const double *Q, int n, double t)
{
    int i;
    if (t < T[0] || t > T[n-1]) return 0.0;
    for (i = 1; i < n; i++)
        if (t <= T[i])
            return Q[i-1] + (t - T[i-1]) * (Q[i] - Q[i-1]) / (T[i] - T[i-1]);
    return 0.0;
}

int main(void)
{
    double elapsed = 0.0, tStart = 0.0, t, q1 = 0.0, q2 = 0.0, e1, e2, dev;
    double maxDev = 0.0, tMax = 0.0, vol = -1.0;
    int rc, j1, j2, m, nBad = 0;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-28_shared-series.inp", "NUM-28_6.rpt", "NUM-28_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    j2 = swmm_node_index(e, "J2");

    printf("  t (min)   J1 inflow    TS1   J2 inflow    TS2   (cfs)\n");
    while (!rc)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0) break;
        t = tStart * 1440.0;
        swmm_node_get_lateral_inflow(e, j1, &q1);
        swmm_node_get_lateral_inflow(e, j2, &q2);
        e1 = series(T1, Q1, 8, t);
        e2 = series(T2, Q2, 5, t);
        dev = fmax(fabs(q1 - e1), fabs(q2 - e2));
        if (dev > 0.001) nBad++;
        if (dev > maxDev) { maxDev = dev; tMax = t; }
        m = (int)floor(t + 0.5);
        if (m == 0 || m == 1 || m == 5 || m == 8 || m == 10 || m == 14 || m == 16
            || m == 17 || m == 20 || m == 40 || m == 61 || m == 80)
            printf("  %7d   %9.4f %7.4f   %9.4f %7.4f\n", m, q1, e1, q2, e2);
        tStart = elapsed;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_routing_total(e, SWMM_ROUTING_EXTERNAL, &vol);   /* ft3 */
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    printf("Routing steps with a wrong inflow: %d, largest error %.4f cfs at %.0f min\n",
           nBad, maxDev, tMax);
    printf("External inflow volume: %.0f ft3 = %.3f acre-ft, integral of TS1 + TS2 %.0f ft3 = %.3f acre-ft\n",
           vol, vol / 43560.0, EXACT_VOLUME, EXACT_VOLUME / 43560.0);

    if (nBad > 0 || fabs(vol - EXACT_VOLUME) > 0.01 * EXACT_VOLUME)
    {
        printf("FAIL: %d routing steps use a wrong series value (up to %.2f cfs off at "
               "%.0f min); external inflow %.0f ft3 instead of %.0f\n",
               nBad, maxDev, tMax, vol, EXACT_VOLUME);
        return 1;
    }
    printf("PASS: every routing step applies the series value at its start time and the "
           "external inflow volume matches the integral\n");
    return 0;
}
