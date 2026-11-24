/*
 * NUM-60: a bottom orifice below tailwater is corrected for submergence twice.
 *
 * Two storage units (1000 ft2, crest at their invert) drain through identical
 * bottom orifices (circular, 1 ft, Cd 0.65) to fixed outfalls whose stage is
 * 1 ft above the crest. S1 receives 1.25 cfs and S2 2.2 cfs. After 2 hours
 * both are at steady state and pass their inflow.
 *
 * SWMM's bottom orifice turns from weir to orifice flow at a head H* where the
 * two formulas give the same flow (orifice_getWeirCoeff):
 *   H*    = (Cd / 0.414) * area / perimeter = 0.3925 ft
 *   cWeir = Cd * A * sqrt(2 g) * sqrt(H*)   = 2.567 cfs = orifice flow at H*
 * With the tailwater above the crest the head on the orifice is the head
 * difference dh = h1 - h2, so below H* the flow is cWeir * (dh / H*)^1.5 and
 * at H* it meets the orifice formula. The steady head difference is therefore
 *   dh = H* * (Q / cWeir)^(2/3)
 * S1: 0.2430 ft; S2: 0.3542 ft. Correct behaviour: each unit's final depth is
 * 1 ft + dh (tolerance 0.005 ft) and its orifice flow holds still over the
 * last hour (range below 0.01 cfs).
 *
 * The defect: orifice_getInflow() also multiplies that flow by Villemonte's
 * submergence factor, which assumes the head was measured from the crest, not
 * from the tailwater. The flow is too small below H* and jumps by the inverse
 * of the factor at H* (1.79 to 2.57 cfs here). S1 settles too high; S2's
 * inflow lies inside the jump, so no depth passes it: the level sits at H*
 * and the orifice flow switches between the two sides of the jump.
 */
#include <math.h>
#include <stdio.h>
#include "swmm5.h"

#define N 2

int main(void)
{
    const char* unit[N] = {"S1", "S2"};
    const double inflow[N] = {1.25, 2.2};
    const double cd = 0.65, g = 32.2, stage = 1.0;
    const double area = 3.14159265358979 / 4.0;
    const double hstar = cd / 0.414 * 0.25;
    const double cweir = cd * area * sqrt(2.0 * g) * sqrt(hstar);
    double depth[N], flow[N], lo[N], hi[N], qlo[N], qhi[N], expect, t = 0.0;
    char id[8];
    int i, err, bad = 0;

    for (i = 0; i < N; i++) { lo[i] = qlo[i] = 1e9; hi[i] = qhi[i] = -1e9; }
    err = swmm_open("NUM-60_bottom-orifice.inp", "NUM-60.rpt", "NUM-60.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        for (i = 0; i < N; i++)
        {
            depth[i] = swmm_getValue(swmm_NODE_DEPTH, swmm_getIndex(swmm_NODE, unit[i]));
            sprintf(id, "OR%d", i + 1);
            flow[i] = swmm_getValue(swmm_LINK_FLOW, swmm_getIndex(swmm_LINK, id));
            if (t > 1.0 / 24.0)
            {
                if (depth[i] < lo[i]) lo[i] = depth[i];
                if (depth[i] > hi[i]) hi[i] = depth[i];
                if (flow[i] < qlo[i]) qlo[i] = flow[i];
                if (flow[i] > qhi[i]) qhi[i] = flow[i];
            }
        }
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("Orifice flow with the tailwater 1 ft above the crest: %.3f cfs just "
           "below H* = %.4f ft, %.3f cfs at H*\n",
           cweir * pow(1.0 - pow(stage / (stage + hstar), 1.5), 0.385), hstar, cweir);
    printf("Unit  Inflow (cfs)  Final depth (ft)  Expected (ft)  Depth range (ft)  "
           "Orifice flow range (cfs)\n");
    for (i = 0; i < N; i++)
    {
        expect = stage + hstar * pow(inflow[i] / cweir, 2.0 / 3.0);
        printf("%-4s  %12.3f  %16.4f  %13.4f  %16.4f  %11.3f - %-10.3f%s\n", unit[i],
               inflow[i], depth[i], expect, hi[i] - lo[i], qlo[i], qhi[i],
               (fabs(depth[i] - expect) > 0.005 || qhi[i] - qlo[i] > 0.01) ? "   <--" : "");
        if (fabs(depth[i] - expect) > 0.005 || qhi[i] - qlo[i] > 0.01) bad++;
    }
    if (bad)
    {
        printf("FAIL: %d of %d units do not settle at the head difference the bottom "
               "orifice's own weir and orifice formulas give\n", bad, N);
        return 1;
    }
    printf("PASS: below tailwater each bottom orifice passes its inflow at the head "
           "difference its weir and orifice formulas give\n");
    return 0;
}
