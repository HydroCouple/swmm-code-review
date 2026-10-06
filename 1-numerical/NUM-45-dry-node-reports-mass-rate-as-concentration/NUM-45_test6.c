/*
 * NUM-45 for 6.0.0: a dry node with no inflow reports its pollutant mass
 * inflow rate as its concentration.
 *
 * 6.0.0 copies the 5.3.0 behaviour on purpose: in the non-storage branch of
 * the node quality kernel a dry node with no inflow publishes its summed
 * load rate (qual_mass_in) as its concentration.
 *
 * Deck: junctions J2 and J3 receive MASS loads of 10 and 100 mg/s of P1 but
 * no water, so they stay at depth 0 with zero inflow for the whole run.
 *
 * Correct behaviour, as in NUM-45_test.c: a node that holds no water and
 * receives none has no pollutant concentration; SWMM reports it as 0. The
 * test reads depth, inflow and P1 at J2 and J3 after every routing step and
 * requires P1 = 0 (within 1e-6 mg/L) whenever depth and inflow are 0. The
 * bug gives 0.353 and 3.53 mg/L: the load in internal units, mg/s divided by
 * 28.317 L/ft3.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

#define LPERFT3 28.317       /* SWMM's litres per ft3 (consts.h) */

int main(void)
{
    const char *names[2] = {"J2", "J3"};
    const double load[2] = {10.0, 100.0};      /* mg/s, from [INFLOWS] */
    double t = 0.0, cmax[2] = {0.0, 0.0}, ymax[2] = {0.0, 0.0}, qmax[2] = {0.0, 0.0};
    int rc, k, idx[2], nsteps = 0, nbad = 0;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "NUM-45_dry-junction-mass-load.inp", "NUM-45_6.rpt", "NUM-45_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    for (k = 0; k < 2 && !rc; k++) idx[k] = swmm_node_index(e, names[k]);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (rc || t <= 0.0) break;
        nsteps++;
        for (k = 0; k < 2; k++)
        {
            double y = 0.0, q = 0.0, c = 0.0;
            swmm_node_get_depth(e, idx[k], &y);
            swmm_node_get_inflow(e, idx[k], &q);
            swmm_node_get_quality(e, idx[k], 0, &c);
            if (fabs(y) > ymax[k]) ymax[k] = fabs(y);
            if (fabs(q) > qmax[k]) qmax[k] = fabs(q);
            if (fabs(c) > fabs(cmax[k])) cmax[k] = c;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    printf("Node  MASS load   max depth  max inflow  max P1 reported  load / 28.317 L/ft3\n");
    printf("      (mg/s)      (ft)       (cfs)       (mg/L)           (mg/s per cfs)\n");
    for (k = 0; k < 2; k++)
    {
        int bad = ymax[k] == 0.0 && qmax[k] == 0.0 && fabs(cmax[k]) > 1.0e-6;
        if (bad) nbad++;
        printf("%-4s  %8.1f    %8.4f   %9.4f    %13.6f    %13.6f%s\n", names[k], load[k], ymax[k],
               qmax[k], cmax[k], load[k] / LPERFT3, bad ? "  <-- wrong" : "");
    }
    printf("(%d routing steps)\n", nsteps);
    if (nbad)
    {
        printf("FAIL: %d of 2 dry nodes with no inflow report a non-zero concentration "
               "(J2 %.6f, J3 %.6f mg/L), equal to their mass load rate\n", nbad, cmax[0], cmax[1]);
        return 1;
    }
    printf("PASS: nodes with no water and no inflow report a zero concentration\n");
    return 0;
}
