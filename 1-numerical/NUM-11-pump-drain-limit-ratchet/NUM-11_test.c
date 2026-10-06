/*
 * NUM-11: a Type2/3/4 pump cannot draw down a junction wet well whose
 * conduits add no surface area.
 *
 * getModPumpFlow() (dynwave.c) keeps the pump from emptying a non-storage
 * inlet node:
 *     y = Node[j].oldDepth + netFlowVolume / Xnode[j].newSurfArea;
 *     if ( y <= 0.0 ) return Node[j].inflow;
 * newSurfArea is the area the node's links have added so far, without the
 * MIN_SURFAREA floor that setNodeDepth() applies. Conduit C0 falls freely
 * into J1 and adds no area at J1, so the area is 0. Any net withdrawal gives
 * y = -inf and the pump is pinned to the inflow, whatever the depth.
 *
 * Deck: 6 cfs for 10 min into J1 (above the 5 cfs pump capacity), then
 * 1 cfs; 1 s steps.
 *
 * Correct behaviour: after the inflow drops to 1 cfs, the 5 cfs pump empties
 * J1 (about 125 ft3 at 10 ft over 12.566 ft2) in well under a minute and then
 * pumps the inflow, so from 0:15 on J1 is near empty and P1 ~ 1 cfs.
 *
 * Tolerance: J1 below 0.5 ft at 0:15, 0:30 and 1:00 (with the defect it
 * holds above 8 ft).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double elapsed = 0.0, worst = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, j1, p1, c0, k = 0;
    const double tRep[] = {5, 10, 12, 15, 30, 59};   /* minutes */
    const int nRep = 6;

    err = swmm_open("NUM-11_junction-wet-well.inp", "NUM-11.rpt", "NUM-11.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    p1 = swmm_getIndex(swmm_LINK, "P1");
    c0 = swmm_getIndex(swmm_LINK, "C0");
    printf("  time   C0 inflow  J1 depth   P1 flow\n");
    printf(" (min)       (cfs)      (ft)     (cfs)\n");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        if (k < nRep && elapsed * 1440.0 >= tRep[k] - 1e-6)
        {
            double y = swmm_getValue(swmm_NODE_DEPTH, j1);
            printf("%6.0f %11.3f %9.3f %9.3f\n", tRep[k],
                   swmm_getValue(swmm_LINK_FLOW, c0), y,
                   swmm_getValue(swmm_LINK_FLOW, p1));
            if (tRep[k] >= 15 && y > worst) worst = y;
            k++;
        }
    }
    swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    printf("Routing continuity error: %.3f %%\n", flowErr);
    if (worst > 0.5)
    {
        printf("FAIL: with 1 cfs inflow and a 5 cfs pump, J1 stays at %.3f ft "
               "instead of being drawn down\n", worst);
        return 1;
    }
    printf("PASS: the pump draws J1 down once the inflow drops below its "
           "capacity (J1 at most %.3f ft from 0:15 on)\n", worst);
    return 0;
}
