/*
 * CON-18 for 6.0.0: DWSolver keeps legacy's slot area
 * A(y) = aFull + (y - yFull) * w(y), which shrinks as the depth rises from
 * 1.29 to 1.78 times the full depth and is not the integral of the slot width
 * that node continuity uses.
 *
 * Same deck and checks as CON-18_test.c: a full 3 ft pipe between two 20 ft2
 * storage units, filled by 0.035 cfs for 6 hours with no outflow.
 *  1. The conduit volume (swmm_link_get_volume, every routing step) never
 *     falls below an earlier value by more than 1 ft3.
 *  2. Conduit volume + storage volumes = initial volume + inflow so far,
 *     within 2% of the 6-hour inflow (756 ft3) at every step.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    const double D = 3.0, L = 1000.0, PI = 3.14159265358979;
    const double vFull = PI * D * D / 4.0 * L;
    const double aStor = 20.0, qIn = 0.035;
    const double v0 = vFull + 2.0 * aStor * D;
    const double inTotal = qIn * 6.0 * 3600.0;
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, vol = 0.0, h1 = 0.0, h2 = 0.0, eta, cont = 0.0, flowErr;
    double runMax = -1.0, drop = 0.0, etaDrop = 0.0, inflow, stored, miss, worstMiss = 0.0;
    double nextPrint = 1800.0;
    int rc, j1, j2, c1;

    rc = swmm_engine_open(e, "CON-18_closed-pipe.inp", "CON-18_6.rpt", "CON-18_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    j2 = swmm_node_index(e, "J2");
    c1 = swmm_link_index(e, "C1");

    printf("Time   head / D   conduit volume   inflow so far   stored - initial   missing\n");
    printf("                      (ft3)            (ft3)            (ft3)          (ft3)\n");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);          /* t: elapsed time in days */
        if (t <= 0) break;
        swmm_link_get_volume(e, c1, &vol);     /* ft3 (CFS model) */
        swmm_node_get_head(e, j1, &h1);        /* ft, invert 0 */
        swmm_node_get_head(e, j2, &h2);
        eta = 0.5 * (h1 + h2) / D;
        if (vol > runMax) runMax = vol;
        if (runMax - vol > drop) { drop = runMax - vol; etaDrop = eta; }
        inflow = qIn * t * 86400.0;
        stored = vol + aStor * (h1 + h2) - v0;
        miss = inflow - stored;
        if (fabs(miss) > fabs(worstMiss)) worstMiss = miss;
        if (t * 86400.0 >= nextPrint - 1.0)
        {
            printf("%d:%02d    %5.3f      %8.1f        %8.1f          %8.1f       %7.1f\n",
                   (int)(t * 24.0 + 1e-6), (int)(t * 1440.0 + 1e-6) % 60, eta, vol,
                   inflow, stored, miss);
            nextPrint += 1800.0;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_routing_continuity_error(e, &cont);   /* a fraction */
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    flowErr = 100.0 * cont;
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

    printf("Largest fall of the conduit volume below an earlier value: %.1f ft3 "
           "(reached at %.3f D)\n", drop, etaDrop);
    printf("Largest volume missing from storage: %.1f ft3 (%.1f%% of the 6-hour inflow)\n",
           worstMiss, 100.0 * worstMiss / inTotal);
    printf("SWMM routing continuity error: %.3f%%\n", flowErr);

    /* 1 ft3 and 2% of the inflow; the bug gives ~150 ft3 and ~60% */
    if (drop > 1.0 || fabs(worstMiss) > 0.02 * inTotal)
    {
        printf("FAIL: the conduit volume falls by %.1f ft3 while the head rises, and up to "
               "%.1f ft3 of the inflow is missing from storage (continuity error %.3f%%)\n",
               drop, worstMiss, flowErr);
        return 1;
    }
    printf("PASS: the conduit volume rises with the head and storage accounts for "
           "the inflow (continuity error %.3f%%)\n", flowErr);
    return 0;
}
