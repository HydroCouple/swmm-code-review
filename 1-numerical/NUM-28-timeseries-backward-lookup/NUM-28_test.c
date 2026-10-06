/*
 * NUM-28: table_tseriesLookup() interpolates from the wrong bracket after a
 * lookup goes back in time to the start of a series.
 *
 * In NUM-28_shared-series.inp the series TS1 and TS2 are each read by two
 * consumers on different clocks: an EXT buildup function, read at the END of
 * each 18-minute runoff step, and a FLOW inflow (TS1 at J1, TS2 at J2), read at
 * the START of each 1-minute routing step. The first routing lookup after each
 * runoff step is therefore earlier than the last lookup. TS1 starts with the
 * run, so such a lookup lands in its first interval; TS2 starts 5 minutes in,
 * so early lookups land before its first entry.
 *
 * Correct behaviour: the inflow the engine applies at J1 (J2) in a routing step
 * is TS1 (TS2) interpolated linearly at the step's start time (there is no
 * rain, so it is the node's whole lateral inflow), and the External Inflow
 * volume in the routing continuity table is the integral of the two series:
 * 58,500 + 18,000 = 76,500 ft3 = 1.756 acre-ft.
 *
 * Tolerances: the routing steps fall on whole minutes. The engine looks the
 * series up 1 ms after the step's start (getDateTime() adds 1 ms), which moves
 * the value by at most 7e-5 cfs on the steepest segment (TS2, 4 cfs/min), so
 * 0.001 cfs is ample; the bug gives errors of several cfs. The volume is
 * integrated over 1-minute steps of piecewise-linear series, which is within
 * 0.1 % of the integral; 1 % separates it from the buggy +11 %.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* the deck's series: times in minutes, flows in cfs */
static const double T1[] = {0, 15, 30, 45, 60, 75, 90, 105};
static const double Q1[] = {0, 10, 20, 0, 0, 30, 5, 0};
static const double T2[] = {5, 10, 15, 20, 30};
static const double Q2[] = {0, 5, 10, 30, 0};
#define EXACT_VOLUME 76500.0   /* ft3: sum of the trapezoids of TS1 and TS2 */

static double series(const double *T, const double *Q, int n, double t)
{
    int i;
    if (t < T[0] || t > T[n-1]) return 0.0;
    for (i = 1; i < n; i++)
        if (t <= T[i])
            return Q[i-1] + (t - T[i-1]) * (Q[i] - Q[i-1]) / (T[i] - T[i-1]);
    return 0.0;
}

/* External Inflow volume (acre-ft) from the Flow Routing Continuity table */
static double reportVolume(const char *rpt)
{
    char line[256];
    double v = -1.0;
    int inRouting = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Flow Routing Continuity")) inRouting = 1;
        if (inRouting && strstr(line, "External Inflow"))
        {
            char *p = strstr(line, "....");        /* skip the dotted leader */
            while (p && *p == '.') p++;
            if (p) sscanf(p, "%lf", &v);
            break;
        }
    }
    fclose(f);
    return v;
}

int main(void)
{
    double elapsed = 0.0, tStart = 0.0, t, q1, q2, e1, e2, dev;
    double maxDev = 0.0, tMax = 0.0, vol, volExact = EXACT_VOLUME / 43560.0;
    int err, j1, j2, m, nBad = 0;

    err = swmm_open("NUM-28_shared-series.inp", "NUM-28.rpt", "NUM-28.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    j2 = swmm_getIndex(swmm_NODE, "J2");

    printf("  t (min)   J1 inflow    TS1   J2 inflow    TS2   (cfs)\n");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        /* the inflow of this step was looked up at the step's start time */
        t = tStart * 1440.0;
        q1 = swmm_getValue(swmm_NODE_LATFLOW, j1);
        q2 = swmm_getValue(swmm_NODE_LATFLOW, j2);
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
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    vol = reportVolume("NUM-28.rpt");
    printf("Routing steps with a wrong inflow: %d, largest error %.4f cfs at %.0f min\n",
           nBad, maxDev, tMax);
    printf("External Inflow volume: report %.3f acre-ft, integral of TS1 + TS2 %.3f acre-ft\n",
           vol, volExact);

    if (nBad > 0 || fabs(vol - volExact) > 0.01 * volExact)
    {
        printf("FAIL: %d routing steps use a wrong series value (up to %.2f cfs off at "
               "%.0f min); External Inflow %.3f acre-ft instead of %.3f\n",
               nBad, maxDev, tMax, vol, volExact);
        return 1;
    }
    printf("PASS: every routing step applies the series value at its start time and the "
           "External Inflow volume matches the integral\n");
    return 0;
}
