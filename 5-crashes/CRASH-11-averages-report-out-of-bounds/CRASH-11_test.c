/*
 * CRASH-11: with [REPORT] AVERAGES YES, 5.3.0's output_saveAvgResults() reads
 * past the end of AvgNodeResults when only some nodes are reported, and
 * converts the averaged depth to user units a second time.
 *
 * Since 5.3.0 (fork commit 7266536f, #188) "Reported Max Depth" in the Node
 * Depth Summary is taken from the period-averaged depths:
 *     for (i = 0; i < Nobjects[NODE]; i++)
 *         stats_updateMaxNodeDepth(i, AvgNodeResults[i].xAvg[NODE_DEPTH] * UCF(LENGTH) / Nsteps);
 * AvgNodeResults has one entry per REPORTED node (NumNodes of them, filled in
 * reported-node order), so with [REPORT] NODES listing a subset, i runs past
 * the end of the block and the xAvg pointer read there is dereferenced; and
 * xAvg already holds node_getResults() values in user units, so SI depths
 * come out multiplied by 0.3048.
 *
 * Correct behaviour: "Reported Max Depth" is the largest depth saved at the
 * reporting times. Both decks have a one-hour plateau of steady flow, so the
 * point and averaged depths at the reporting times agree there and the
 * maximum of the depths in the .out is the reference for every engine.
 *   1. CRASH-11_si-all.inp (CMS, all nodes reported): Reported Max Depth of
 *      J1-J3 = maximum of the node's depth series in the .out.
 *   2. CRASH-11_si-subset.inp (same model, NODES J3 only): the run completes
 *      and J1-J3 report the same maxima as in run 1.
 * Tolerance 0.01 m + 3 %: the .rpt prints two decimals; the bug gives
 * 0.3048 x the depth (0.09 m for 0.31 m) or an out-of-bounds read.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

static const char *ids[3] = {"J1", "J2", "J3"};

static int run(const char *inp, const char *rpt, const char *out)
{
    double t = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    return err;
}

#define RPT1 "CRASH-11_all.rpt"
#define OUT1 "CRASH-11_all.out"
#define RPT2 "CRASH-11_subset.rpt"
#define OUT2 "CRASH-11_subset.out"
#include "CRASH-11_common.h"
