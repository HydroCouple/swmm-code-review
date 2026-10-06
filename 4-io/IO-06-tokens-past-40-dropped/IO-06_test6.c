/*
 * IO-06 for 6.0.0: tokens past the 40th on an input line.
 *
 * Legacy getTokens() keeps at most 40 tokens per line (see IO-06_test.c).
 * 6.0.0's Tokenizer has no such limit, and handle_curves() reads every
 * depth/area pair on a line.
 *
 * Correct behaviour, as in IO-06_test.c: with 1 cfs into SU1 for 1 hour and
 * no outflow, SU1 holds 3600 ft3 and its depth at 1:00 is 11.925 ft (curve
 * SC1: 100 ft2 up to 9 ft, 1000 ft2 from 9.5 ft). Tolerance 0.05 ft.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    double t = 0.0, depth = -1.0, maxDepth = 0.0, weirVol = 0.0, tPrev = 0.0, q = 0.0;
    int err, su1 = -1, w1 = -1;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "IO-06_one-line-curve.inp", "IO-06_6.rpt", "IO-06_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 0);
    if (!err)
    {
        su1 = swmm_node_index(e, "SU1");
        w1 = swmm_link_index(e, "W1");
    }
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        swmm_node_get_depth(e, su1, &depth);
        if (depth > maxDepth) maxDepth = depth;
        swmm_link_get_flow(e, w1, &q);
        if (t > 0.0) { weirVol += q * (t - tPrev) * 86400.0; tPrev = t; }
        if (!(t > 0.0)) break;
    }
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    printf("SU1 depth at 1:00   expected 11.925 ft, got %.3f ft (max %.3f ft)\n", depth, maxDepth);
    printf("weir W1 outflow     expected 0 ft3,      got %.0f ft3\n", weirVol);
    printf("error code %d\n", err);
    if (err || fabs(depth - 11.925) > 0.05)
    {
        printf("FAIL: storage curve written on one line is cut after 40 tokens "
               "(depth %.3f ft instead of 11.925 ft)\n", depth);
        return 1;
    }
    printf("PASS: all 41 points of the one-line storage curve are read "
           "(depth %.3f ft at 1:00)\n", depth);
    return 0;
}
