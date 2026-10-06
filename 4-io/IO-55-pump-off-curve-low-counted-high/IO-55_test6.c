/*
 * IO-55 for 6.0.0: the Pumping Summary's "% Time Off Pump Curve" columns.
 *
 * Same check as IO-55_test.c. 6.0.0 does not misbook the time as legacy
 * does: it does not track off-curve operation at all and prints 0.0 in
 * both columns for every pump.
 *
 * The deck has two storage wet wells with the same inflow (1.5 cfs for 2 h,
 * then 0.5 cfs), each drained by a Type2 (depth) pump that delivers 1 cfs at
 * every depth on its curve. W1 rises from 0 to 3.6 ft and falls to 0.9 ft.
 * P1's curve covers 2 to 6 ft, so W1 is below it about half the time and
 * never above it. P2's curve covers 0.5 to 1.5 ft, so W2 is below it for
 * the first minutes and above it for most of the run.
 *
 * The test steps the run and, for each step in which a pump runs (flow above
 * 0.001 cfs, the threshold stats.c uses), books the step's length as below
 * the curve (wet-well depth < first curve depth), above it (> last curve
 * depth) or on it. It then reads the "% Time Off Pump Curve Low / High"
 * columns of the Pumping Summary.
 *
 * Correct behaviour: Low = time below the curve / running time and High =
 * time above it / running time. The tolerance of 2 percentage points covers
 * the depth the pump sees during the step versus the depth at its end, at
 * the two crossings of each threshold; the defect moves P1's 49 % below
 * the curve entirely into High.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_nodes.h"

#define NP 2

int main(void)
{
    const char *pumpId[NP] = { "P1", "P2" };
    const char *wellId[NP] = { "W1", "W2" };
    const double xMin[NP] = { 2.0, 0.5 }, xMax[NP] = { 6.0, 1.5 };
    double on[NP] = { 0 }, below[NP] = { 0 }, above[NP] = { 0 };
    double rptLow[NP] = { -1, -1 }, rptHigh[NP] = { -1, -1 };
    double t = 0.0, prev = 0.0, dt, q, y, v[7];
    int link[NP], node[NP], err, i, k, starts, inTable = 0, nFound = 0, ok = 1;
    char line[512], name[64];
    FILE *f;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "IO-55_two-wells.inp", "IO-55_6.rpt", "IO-55_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    for (k = 0; k < NP; k++)
    {
        link[k] = swmm_link_index(e, pumpId[k]);
        node[k] = swmm_node_index(e, wellId[k]);
    }
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0) break;
        dt = (t - prev) * 86400.0;
        prev = t;
        for (k = 0; k < NP; k++)
        {
            swmm_link_get_flow(e, link[k], &q);
            swmm_node_get_depth(e, node[k], &y);
            if (q <= 0.001) continue;
            on[k] += dt;
            if (y < xMin[k]) below[k] += dt;
            if (y > xMax[k]) above[k] += dt;
        }
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    f = fopen("IO-55_6.rpt", "r");
    if (!f) { printf("FAIL: cannot read IO-55_6.rpt\n"); return 1; }
    while (fgets(line, sizeof line, f) && nFound < NP)
    {
        if (strstr(line, "Pumping Summary")) inTable = 1;
        if (!inTable) continue;
        if (sscanf(line, "%63s %lf %d %lf %lf %lf %lf %lf %lf %lf", name, &v[0],
                   &starts, &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &y) != 10)
            continue;
        for (k = 0; k < NP; k++)
            if (strcmp(name, pumpId[k]) == 0)
            {
                rptLow[k] = v[6];
                rptHigh[k] = y;
                nFound++;
            }
    }
    fclose(f);
    if (nFound < NP) { printf("FAIL: pump rows missing from the Pumping Summary\n"); return 1; }

    printf("        curve depths   %% time below curve    %% time above curve\n");
    printf("pump        (ft)       wet well   report      wet well   report\n");
    for (k = 0; k < NP; k++)
    {
        below[k] = below[k] / on[k] * 100.0;
        above[k] = above[k] / on[k] * 100.0;
        printf("%-4s     %.1f - %.1f     %7.1f  %7.1f       %7.1f  %7.1f\n", pumpId[k],
               xMin[k], xMax[k], below[k], rptLow[k], above[k], rptHigh[k]);
    }
    for (k = 0; k < NP; k++)
        for (i = 0; i < 2; i++)
        {
            double want = i ? above[k] : below[k], got = i ? rptHigh[k] : rptLow[k];
            if (got < want - 2.0 || got > want + 2.0)
            {
                printf("  %s: report %s = %.1f %%, wet well was %s the curve %.1f %% of the time\n",
                       pumpId[k], i ? "High" : "Low", got, i ? "above" : "below", want);
                ok = 0;
            }
        }
    if (!ok)
    {
        printf("FAIL: the %% Time Off Pump Curve columns do not match the time the "
               "wet wells spent below and above the pump curves\n");
        return 1;
    }
    printf("PASS: %% Time Off Pump Curve Low and High match the time the wet wells "
           "spent below and above the pump curves\n");
    return 0;
}
