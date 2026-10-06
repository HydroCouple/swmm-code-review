/*
 * IO-38: once an external time-series file has reached end-of-file,
 * table_tseriesLookup() returns the wrong value.
 *
 * A. IO-38_stage-file.inp: outfalls O1 and O2 take their stage from two files
 *    with the same three lines (99.5, 100.5, 101.5 ft at 0:00, 1:00, 2:00).
 *    Only O2's file ends with a line break. Outfall stage series are extended
 *    beyond their last entry, so from 2:00 to the end of the 3-hour run both
 *    stages must be 101.5 ft, a depth of 2.5 ft above the 99-ft inverts.
 *    Before 2:00 the depth is the interpolated stage minus 99 ft (the 1 cfs
 *    flow keeps critical depth below 0.5 ft).
 *
 * B. IO-38_shared-file-series.inp: the file series FLW (ending with a line
 *    break) is read by an EXT buildup function at the end of each 18-minute
 *    runoff step and by J1's FLOW inflow at the start of each 1-minute routing
 *    step. J1's lateral inflow in each routing step must be FLW interpolated
 *    at the step's start (no rain, so no runoff), and the External Inflow
 *    volume must be FLW's integral, 30,450 ft3 = 0.699 acre-ft.
 *
 * Tolerances: outfall stages are looked up at the exact end of each step, so
 * 0.001 ft is ample (the bug is off by 1 ft). Inflows are looked up 1 ms after
 * the step start (getDateTime() adds 1 ms), worth at most 2e-5 cfs on FLW's
 * steepest segment, so 0.001 cfs is ample (the bug is off by up to 10 cfs).
 * The engine adds up each 1-minute step's inflow, which lands 0.4 % above the
 * integral for this series (it ends with a steep drop); 1 % separates that from
 * the bug's -16 %.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* FLW (IO-38_flow.dat): minutes, cfs */
static const double TF[] = {0, 30, 45, 60, 75, 85};
static const double QF[] = {5, 5, 5, 8, 10, 0};
#define NF 6
#define EXACT_VOLUME 30450.0   /* ft3 */

static double flw(double t)
{
    int i;
    if (t < TF[0] || t > TF[NF-1]) return 0.0;
    for (i = 1; i < NF; i++)
        if (t <= TF[i])
            return QF[i-1] + (t - TF[i-1]) * (QF[i] - QF[i-1]) / (TF[i] - TF[i-1]);
    return 0.0;
}

/* stage of both outfall files (hours -> ft), held at 101.5 after 2:00 */
static double stage(double h)
{
    return (h >= 2.0) ? 101.5 : 99.5 + h;
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
    double elapsed = 0.0, tStart = 0.0, h, m, d1, d2, expect, q, e;
    double maxA = 0.0, maxB = 0.0, tMaxB = 0.0, vol, volExact = EXACT_VOLUME / 43560.0;
    int err, o1, o2, j1, badA1 = 0, badA2 = 0, badB = 0, ok;

    /* ---- A: outfall stage files with and without a final line break */
    err = swmm_open("IO-38_stage-file.inp", "IO-38_stage.rpt", "IO-38_stage.out");
    if (!err) err = swmm_start(1);
    o1 = swmm_getIndex(swmm_NODE, "O1");
    o2 = swmm_getIndex(swmm_NODE, "O2");
    printf("A. outfall depths (ft)\n");
    printf("  time (h)   O1 (no final newline)   O2 (final newline)   expected\n");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        h = elapsed * 24.0;                 /* the stage is set at the step's end */
        d1 = swmm_getValue(swmm_NODE_DEPTH, o1);
        d2 = swmm_getValue(swmm_NODE_DEPTH, o2);
        expect = stage(h) - 99.0;
        if (fabs(d1 - expect) > 0.001) badA1++;
        if (fabs(d2 - expect) > 0.001) badA2++;
        maxA = fmax(maxA, fmax(fabs(d1 - expect), fabs(d2 - expect)));
        m = floor(h * 360.0 + 0.5);         /* 10-second steps */
        if (m == 180 || m == 540 || m == 720 || m == 721 || m == 900 || m == 1080)
            printf("  %8.3f   %21.4f   %18.4f   %8.4f\n", h, d1, d2, expect);
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the stage run stopped with error %d\n", err);
        return 1;
    }
    printf("  steps with a wrong depth: O1 %d, O2 %d (largest error %.4f ft)\n\n",
           badA1, badA2, maxA);

    /* ---- B: a file series read back in time after end-of-file */
    elapsed = 0.0;
    err = swmm_open("IO-38_shared-file-series.inp", "IO-38_shared.rpt", "IO-38_shared.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    printf("B. J1 inflow (cfs)\n");
    printf("  t (min)   J1 inflow       FLW\n");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        m = floor(tStart * 1440.0 + 0.5);   /* the inflow is set at the step's start */
        q = swmm_getValue(swmm_NODE_LATFLOW, j1);
        e = flw(tStart * 1440.0);
        if (fabs(q - e) > 0.001) badB++;
        if (fabs(q - e) > maxB) { maxB = fabs(q - e); tMaxB = m; }
        if (m == 54 || m == 71 || m == 72 || m == 75 || m == 80 || m == 84 || m == 86)
            printf("  %7.0f   %9.4f   %7.4f\n", m, q, e);
        tStart = elapsed;
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the shared-series run stopped with error %d\n", err);
        return 1;
    }
    vol = reportVolume("IO-38_shared.rpt");
    printf("  steps with a wrong inflow: %d (largest error %.4f cfs at %.0f min)\n",
           badB, maxB, tMaxB);
    printf("  External Inflow: report %.3f acre-ft, integral of FLW %.3f acre-ft\n",
           vol, volExact);

    ok = (badA1 == 0 && badA2 == 0 && badB == 0 && fabs(vol - volExact) <= 0.01 * volExact);
    if (!ok)
    {
        printf("FAIL: wrong values after end-of-file: %d steps of O1's depth and %d of O2's "
               "(up to %.2f ft off), %d steps of J1's inflow (up to %.2f cfs off), "
               "External Inflow %.3f instead of %.3f acre-ft\n",
               badA1, badA2, maxA, badB, maxB, vol, volExact);
        return 1;
    }
    printf("PASS: file series give the last value after their end, whatever the final "
           "line break, and are read correctly back in time after end-of-file\n");
    return 0;
}
