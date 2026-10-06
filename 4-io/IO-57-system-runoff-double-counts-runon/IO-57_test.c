/*
 * IO-57: the system "Runoff" and "Total lateral inflow" series in the .out
 * count runoff that one subcatchment routes onto another twice.
 *
 * output_saveSubcatchResults() adds every subcatchment's runoff to
 * SYS_RUNOFF, also when its outlet is another subcatchment, whose own runoff
 * already contains that runon; output_saveResults() then sets
 *     SYS_INFLOW = SYS_RUNOFF + SYS_DWFLOW + SYS_GWFLOW + SYS_IIFLOW + SYS_EXFLOW.
 * The runoff continuity table counts the water once: massbal books a
 * subcatchment's runoff only if its outlet is a node (runoff.c / subcatch.c).
 *
 * Correct behaviour, in IO-57_runon.inp (S1 drains onto S2, S2 drains to J1;
 * runoff is the only inflow):
 *   1. system runoff = runoff of S2, the only subcatchment draining to a
 *      node (tolerance 0.1 % + 0.001 cfs: both are REAL4 copies of the same
 *      number when right)
 *   2. the volume of the system total lateral inflow over the run = the volume
 *      of the node lateral inflows (tolerance 3 %: compared as volumes
 *      because node lateral inflow lags the subcatchment runoff by one runoff
 *      step, which shifts single periods by up to 17 % on the rising and
 *      falling limbs but cancels over the run)
 * Check 1 is made at every reporting period. The bug overstates both by S1's
 * runoff, 50 % of S2's at the peak.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "swmm5.h"

#define INP "IO-57_runon.inp"
#define RPT "IO-57.rpt"
#define OUT "IO-57.out"

static int run(void)
{
    double t = 0.0;
    int err = swmm_open(INP, RPT, OUT);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    return err;
}

#include "IO-57_check.h"
