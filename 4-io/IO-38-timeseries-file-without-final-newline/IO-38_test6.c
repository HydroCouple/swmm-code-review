/*
 * IO-38 for 6.0.0: the same checks as IO-38_test.c through the 6.0.0 C API.
 * 6.0.0 reads external series files into memory (TableData.cpp), so it is
 * expected to pass.
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
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

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

int main(void)
{
    double elapsed = 0.0, tStart = 0.0, h, m, d1 = 0.0, d2 = 0.0, expect, q = 0.0, e;
    double maxA = 0.0, maxB = 0.0, tMaxB = 0.0, vol = -1.0, volExact = EXACT_VOLUME / 43560.0;
    int err, o1, o2, j1, badA1 = 0, badA2 = 0, badB = 0, ok;
    SWMM_Engine eng = swmm_engine_create();

    /* ---- A: outfall stage files with and without a final line break */
    err = swmm_engine_open(eng, "IO-38_stage-file.inp", "IO-38_stage6.rpt", "IO-38_stage6.out", NULL);
    if (!err) err = swmm_engine_initialize(eng);
    if (!err) err = swmm_engine_start(eng, 1);
    o1 = swmm_node_index(eng, "O1");
    o2 = swmm_node_index(eng, "O2");
    printf("A. outfall depths (ft)\n");
    printf("  time (h)   O1 (no final newline)   O2 (final newline)   expected\n");
    while (!err)
    {
        err = swmm_engine_step(eng, &elapsed);
        if (elapsed <= 0.0) break;
        h = elapsed * 24.0;                 /* the stage is set at the step's end */
        swmm_node_get_depth(eng, o1, &d1);
        swmm_node_get_depth(eng, o2, &d2);
        expect = stage(h) - 99.0;
        if (fabs(d1 - expect) > 0.001) badA1++;
        if (fabs(d2 - expect) > 0.001) badA2++;
        maxA = fmax(maxA, fmax(fabs(d1 - expect), fabs(d2 - expect)));
        m = floor(h * 360.0 + 0.5);         /* 10-second steps */
        if (m == 180 || m == 540 || m == 720 || m == 721 || m == 900 || m == 1080)
            printf("  %8.3f   %21.4f   %18.4f   %8.4f\n", h, d1, d2, expect);
    }
    if (!err) err = swmm_engine_end(eng);
    if (!err) err = swmm_engine_report(eng);
    swmm_engine_close(eng);
    swmm_engine_destroy(eng);
    if (err)
    {
        printf("FAIL: the stage run stopped with error %d\n", err);
        return 1;
    }
    printf("  steps with a wrong depth: O1 %d, O2 %d (largest error %.4f ft)\n\n",
           badA1, badA2, maxA);

    /* ---- B: a file series read back in time after end-of-file */
    elapsed = 0.0;
    eng = swmm_engine_create();
    err = swmm_engine_open(eng, "IO-38_shared-file-series.inp", "IO-38_shared6.rpt", "IO-38_shared6.out", NULL);
    if (!err) err = swmm_engine_initialize(eng);
    if (!err) err = swmm_engine_start(eng, 1);
    j1 = swmm_node_index(eng, "J1");
    printf("B. J1 inflow (cfs)\n");
    printf("  t (min)   J1 inflow       FLW\n");
    while (!err)
    {
        err = swmm_engine_step(eng, &elapsed);
        if (elapsed <= 0.0) break;
        m = floor(tStart * 1440.0 + 0.5);   /* the inflow is set at the step's start */
        swmm_node_get_lateral_inflow(eng, j1, &q);
        e = flw(tStart * 1440.0);
        if (fabs(q - e) > 0.001) badB++;
        if (fabs(q - e) > maxB) { maxB = fabs(q - e); tMaxB = m; }
        if (m == 54 || m == 71 || m == 72 || m == 75 || m == 80 || m == 84 || m == 86)
            printf("  %7.0f   %9.4f   %7.4f\n", m, q, e);
        tStart = elapsed;
    }
    if (!err) err = swmm_engine_end(eng);
    swmm_get_routing_total(eng, SWMM_ROUTING_EXTERNAL, &vol);   /* ft3 */
    vol /= 43560.0;
    if (!err) err = swmm_engine_report(eng);
    swmm_engine_close(eng);
    swmm_engine_destroy(eng);
    if (err)
    {
        printf("FAIL: the shared-series run stopped with error %d\n", err);
        return 1;
    }
    printf("  steps with a wrong inflow: %d (largest error %.4f cfs at %.0f min)\n",
           badB, maxB, tMaxB);
    printf("  External inflow: %.3f acre-ft, integral of FLW %.3f acre-ft\n",
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
