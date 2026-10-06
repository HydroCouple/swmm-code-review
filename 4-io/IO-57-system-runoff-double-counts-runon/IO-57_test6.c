/*
 * IO-57 for 6.0.0: same deck and checks as IO-57_test.c (see there), run
 * through swmm_engine_run(). 6.0.0 sums snap.subcatch.runoff over every
 * subcatchment for sys_runoff (SWMMEngine::postOutputSnapshot), as legacy
 * does, while its runoff mass balance applies the outlet-is-a-node rule.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

#define INP "IO-57_runon.inp"
#define RPT "IO-57_6.rpt"
#define OUT "IO-57_6.out"

static int run(void)
{
    return swmm_engine_run(INP, RPT, OUT, NULL);
}

#include "IO-57_check.h"
