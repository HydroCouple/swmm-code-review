/*
 * CRASH-11 for 6.0.0: same decks and checks as CRASH-11_test.c (see there),
 * run through swmm_engine_run().
 *
 * 6.0.0 does not reproduce 5.3.0's averaged "Reported Max Depth": it keeps the
 * largest depth interpolated to each reporting time, for every node, in
 * display units (SWMMEngine::postOutputSnapshot). On the steady plateau of
 * these decks that equals the maximum saved depth, so the test passes; it is
 * here as evidence that 6.0.0 has neither the out-of-bounds read nor the
 * double unit conversion.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

static const char *ids[3] = {"J1", "J2", "J3"};

static int run(const char *inp, const char *rpt, const char *out)
{
    return swmm_engine_run(inp, rpt, out, NULL);
}

#define RPT1 "CRASH-11_all6.rpt"
#define OUT1 "CRASH-11_all6.out"
#define RPT2 "CRASH-11_subset6.rpt"
#define OUT2 "CRASH-11_subset6.out"
#include "CRASH-11_common.h"
