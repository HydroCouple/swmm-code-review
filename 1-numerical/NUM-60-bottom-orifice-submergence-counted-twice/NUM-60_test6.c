/*
 * NUM-60 for 6.0.0: a bottom orifice below tailwater is corrected for
 * submergence twice (HydStructures.cpp, as legacy link.c). Same deck and
 * checks as NUM-60_test.c.
 *
 * Correct behaviour: each unit's final depth is 1 ft + H* (Q / cWeir)^(2/3)
 * (tolerance 0.005 ft) and its orifice flow holds still over the last hour
 * (range below 0.01 cfs); see NUM-60_test.c for the derivation.
 */
#include <math.h>
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

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
    SWMM_Engine e = swmm_engine_create();

    for (i = 0; i < N; i++) { lo[i] = qlo[i] = 1e9; hi[i] = qhi[i] = -1e9; }
    err = swmm_engine_open(e, "NUM-60_bottom-orifice.inp", "NUM-60_6.rpt", "NUM-60_6.out",
                           NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0) break;
        for (i = 0; i < N; i++)
        {
            swmm_node_get_depth(e, swmm_node_index(e, unit[i]), &depth[i]);
            sprintf(id, "OR%d", i + 1);
            swmm_link_get_flow(e, swmm_link_index(e, id), &flow[i]);
            if (t > 1.0 / 24.0)
            {
                if (depth[i] < lo[i]) lo[i] = depth[i];
                if (depth[i] > hi[i]) hi[i] = depth[i];
                if (flow[i] < qlo[i]) qlo[i] = flow[i];
                if (flow[i] > qhi[i]) qhi[i] = flow[i];
            }
        }
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
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
