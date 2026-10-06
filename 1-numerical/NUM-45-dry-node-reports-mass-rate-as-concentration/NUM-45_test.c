/*
 * NUM-45: a dry node with no inflow reports its pollutant mass inflow rate
 * as its concentration.
 *
 * During a routing step Node[].newQual[] first accumulates the node's mass
 * inflow rate (flow x concentration, plus MASS-type and API loads), and
 * findNodeQual() then divides it by the node's inflow. When the inflow is
 * at or below ZERO (1e-10 cfs) and the node is dry, 5.2.4 set the
 * concentration to 0. 5.3.0 dropped that branch, so the node keeps the
 * accumulated rate and publishes it as a concentration.
 *
 * Deck: junctions J2 and J3 receive MASS loads of 10 and 100 mg/s of P1 but
 * no water, so they stay at depth 0 with zero inflow for the whole run.
 *
 * Correct behaviour: a node that holds no water and receives none has no
 * pollutant concentration; SWMM reports it as 0. The test reads depth,
 * inflow and P1 at J2 and J3 for every reporting period of the .out file and
 * requires P1 = 0 (within 1e-6 mg/L) wherever depth and inflow are 0. The
 * bug gives 0.353 and 3.53 mg/L: the load in internal units, mg/s divided by
 * 28.317 L/ft3.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

#define LPERFT3 28.317       /* SWMM's litres per ft3 (consts.h) */

int main(void)
{
    const char *names[2] = {"J2", "J3"};
    const double load[2] = {10.0, 100.0};      /* mg/s, from [INFLOWS] */
    double t = 0.0, cmax[2] = {0.0, 0.0}, ymax[2] = {0.0, 0.0}, qmax[2] = {0.0, 0.0};
    int rc, k, per, nper = 0, nbad = 0;
    SMO_Handle h = NULL;

    rc = swmm_open("NUM-45_dry-junction-mass-load.inp", "NUM-45.rpt", "NUM-45.out");
    if (!rc) rc = swmm_start(1);
    while (!rc)
    {
        rc = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    SMO_init(&h);
    if (SMO_open(h, "NUM-45.out") || SMO_getTimes(h, SMO_numPeriods, &nper) || nper <= 0)
    {
        printf("FAIL: could not read NUM-45.out\n");
        return 1;
    }
    for (per = 0; per < nper; per++)
    {
        for (k = 0; k < 2; k++)
        {
            float *v = NULL;
            int len = 0;
            /* node results: depth, head, volume, lateral inflow, total inflow,
               flooding, then one value per pollutant */
            if (SMO_getNodeResult(h, per, k, &v, &len) == 0 && len > 6)
            {
                if (fabs(v[0]) > ymax[k]) ymax[k] = fabs(v[0]);
                if (fabs(v[4]) > qmax[k]) qmax[k] = fabs(v[4]);
                if (fabs(v[6]) > fabs(cmax[k])) cmax[k] = v[6];
            }
            SMO_free((void **)&v);
        }
    }
    SMO_close(&h);

    printf("Node  MASS load   max depth  max inflow  max P1 reported  load / 28.317 L/ft3\n");
    printf("      (mg/s)      (ft)       (cfs)       (mg/L)           (mg/s per cfs)\n");
    for (k = 0; k < 2; k++)
    {
        int bad = ymax[k] == 0.0 && qmax[k] == 0.0 && fabs(cmax[k]) > 1.0e-6;
        if (bad) nbad++;
        printf("%-4s  %8.1f    %8.4f   %9.4f    %13.6f    %13.6f%s\n", names[k], load[k], ymax[k],
               qmax[k], cmax[k], load[k] / LPERFT3, bad ? "  <-- wrong" : "");
    }
    printf("(%d reporting periods)\n", nper);
    if (nbad)
    {
        printf("FAIL: %d of 2 dry nodes with no inflow report a non-zero concentration "
               "(J2 %.6f, J3 %.6f mg/L), equal to their mass load rate\n", nbad, cmax[0], cmax[1]);
        return 1;
    }
    printf("PASS: nodes with no water and no inflow report a zero concentration\n");
    return 0;
}
