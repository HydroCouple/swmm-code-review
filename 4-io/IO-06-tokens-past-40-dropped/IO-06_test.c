/*
 * IO-06: tokens past the 40th on an input line are dropped without a message.
 *
 * getTokens() (input.c) stores at most MAXTOKS = 40 tokens per line and
 * ignores the rest of the line. The manual sets no such limit: [CURVES],
 * [TIMESERIES] and [TRANSECTS] GR lines may carry any number of pairs.
 *
 * IO-06_one-line-curve.inp writes storage curve SC1 (41 depth/area points,
 * 84 tokens) on one line: area 100 ft2 up to 9 ft, 1000 ft2 from 9.5 ft. A
 * constant 1 cfs flows into storage unit SU1 for 1 hour and nothing leaves
 * below the 25-ft weir crest, so SU1 holds 3600 ft3 at 1:00:
 *     900 ft3 (0-9 ft) + 275 ft3 (9-9.5 ft) + 2425 ft3 at 1000 ft2
 *     -> depth = 9.5 + 2.425 = 11.925 ft.
 *
 * Correct behaviour: the depth at 1:00 is 11.925 ft. The tolerance of 0.05 ft
 * (about 50 ft3, 1.4 % of the volume) allows for the routing's small
 * continuity error; reading only the first 40 tokens keeps 19 points (up to
 * 9 ft), extends the 100 ft2 area upwards and fills SU1 to the weir crest.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, depth = -1.0, maxDepth = 0.0, weirVol = 0.0, tPrev = 0.0, q;
    int err, su1 = -1, w1 = -1;

    err = swmm_open("IO-06_one-line-curve.inp", "IO-06.rpt", "IO-06.out");
    if (!err) err = swmm_start(0);
    if (!err)
    {
        su1 = swmm_getIndex(swmm_NODE, "SU1");
        w1 = swmm_getIndex(swmm_LINK, "W1");
    }
    while (!err)
    {
        err = swmm_step(&t);
        depth = swmm_getValue(swmm_NODE_DEPTH, su1);
        if (depth > maxDepth) maxDepth = depth;
        q = swmm_getValue(swmm_LINK_FLOW, w1);
        if (t > 0.0) { weirVol += q * (t - tPrev) * 86400.0; tPrev = t; }
        if (!(t > 0.0)) break;
    }
    swmm_end();
    swmm_close();

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
