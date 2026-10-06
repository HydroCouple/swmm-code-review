/*
 * CON-02: the MIN_SURFAREA floor overrides a storage unit's own area curve
 * in the dynamic wave depth update, while its volume still comes from the
 * curve.
 *
 * setNodeDepth() (dynwave.c) floors every node's surface area:
 *     surfArea = Xnode[i].newSurfArea;
 *     surfArea = MAX(surfArea, MinSurfArea);       // 12.566 ft2 by default
 *     ...  dy = dV / surfArea;
 * and then books Node[i].newVolume = node_getVolume(i, yNew), which for a
 * storage unit is its curve volume. For a storage unit smaller than
 * MIN_SURFAREA the depth rises as if it had 12.566 ft2 while the volume
 * uses the real area, so most of the inflow disappears.
 *
 * Deck: wet well S1 with a constant 4 ft2 area and no outlet, 0.05 cfs for
 * 10 min, 5 s steps.
 *
 * Correct behaviour (continuity and geometry): S1 ends holding all the
 * water that entered (V = integral of its inflow, ~30 ft3) at depth V / 4 ft2
 * (~7.5 ft). Tolerances: volume within 0.5 ft3 and depth within 0.05 ft;
 * the defect leaves 9.6 ft3 at 2.4 ft.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double elapsed = 0.0, tPrev = 0.0, dt, q, qPrev = 0.0, vIn = 0.0;
    double y = 0.0, v = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, s1, nextReport = 2;

    err = swmm_open("CON-02_small-wet-well.inp", "CON-02.rpt", "CON-02.out");
    if (!err) err = swmm_start(1);
    s1 = swmm_getIndex(swmm_NODE, "S1");
    printf("  time   inflow vol   S1 depth  S1 volume\n");
    printf(" (min)        (ft3)       (ft)      (ft3)\n");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        dt = (elapsed - tPrev) * 86400.0;
        tPrev = elapsed;
        q = swmm_getValue(swmm_NODE_INFLOW, s1);              /* cfs */
        vIn += 0.5 * (qPrev + q) * dt;                         /* ft3 */
        qPrev = q;
        y = swmm_getValue(swmm_NODE_DEPTH, s1);
        v = swmm_getValue(swmm_NODE_VOLUME, s1);
        if (elapsed * 1440.0 >= nextReport - 1e-6)
        {
            printf("%6d %12.2f %10.3f %10.2f\n", nextReport, vIn, y, v);
            nextReport += (nextReport < 10) ? 2 : 5;
        }
    }
    swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    printf("Expected at the end: volume %.2f ft3, depth %.3f ft (4 ft2)\n",
           vIn, vIn / 4.0);
    printf("Routing continuity error: %.3f %%\n", flowErr);
    if (fabs(v - vIn) > 0.5 || fabs(y - vIn / 4.0) > 0.05)
    {
        printf("FAIL: S1 ends at %.3f ft holding %.2f ft3, but %.2f ft3 entered "
               "(%.3f ft over its 4 ft2)\n", y, v, vIn, vIn / 4.0);
        return 1;
    }
    printf("PASS: S1 holds the %.2f ft3 that entered, at %.3f ft\n", v, y);
    return 0;
}
