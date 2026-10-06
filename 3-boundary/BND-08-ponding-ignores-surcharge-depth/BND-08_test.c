/*
 * BND-08: with ALLOW_PONDING and a ponded area, a junction ponds as soon as
 * it exceeds its maximum depth Ymax; its surcharge depth Ysur is ignored.
 *
 * The input file reference ([JUNCTIONS]) defines Ysur as the "maximum
 * additional pressure head above the ground elevation that the junction can
 * sustain under surcharge conditions" and Apond as the "area subjected to
 * surface ponding once water depth exceeds Ymax + Ysur". setNodeDepth()
 * (dynwave.c) instead uses Ymax alone as the ponding threshold when the node
 * can pond:
 *     isPonded = (canPond && Node[i].newDepth > Node[i].fullDepth);
 *     ...
 *     yMax = Node[i].fullDepth;
 *     if ( canPond == FALSE ) yMax += Node[i].surDepth;
 * and node_getPondedArea() switches to Apond above fullDepth.
 *
 * Deck: J1 (Ymax 5 ft, Ysur 10 ft, Apond 1000 ft2) receives 10 cfs; its 1 ft
 * outlet pipe cannot carry it at the rim, so J1 must pressurize.
 *
 * Correct behaviour (documented rule): J1 holds ponded water only while its
 * depth is above Ymax + Ysur = 15 ft. A junction's stored volume in SWMM is
 * its ponded volume, so: whenever J1's volume exceeds 1 ft3, its depth must
 * be at least 15 ft (0.01 ft tolerance). The defect ponds J1 from 5 ft up.
 */
#include <stdio.h>
#include "swmm5.h"

int main(void)
{
    double elapsed = 0.0, y, v, q, yMinPonded = 1.0e9, vMax = 0.0;
    int err, j1, c1, nextReport = 5;

    err = swmm_open("BND-08_bolted-cover-pond.inp", "BND-08.rpt", "BND-08.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    printf("  time   J1 depth   J1 ponded vol   C1 flow\n");
    printf(" (min)       (ft)           (ft3)     (cfs)\n");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        y = swmm_getValue(swmm_NODE_DEPTH, j1);
        v = swmm_getValue(swmm_NODE_VOLUME, j1);
        q = swmm_getValue(swmm_LINK_FLOW, c1);
        if (v > 1.0 && y < yMinPonded) yMinPonded = y;
        if (v > vMax) vMax = v;
        if (elapsed * 1440.0 >= nextReport - 1e-6)
        {
            printf("%6d %10.2f %15.0f %9.2f\n", nextReport, y, v, q);
            nextReport += (nextReport < 20) ? 5 : 10;
        }
    }
    swmm_end();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    if (vMax <= 1.0)
        printf("J1 never ponds\n");
    else
        printf("Lowest J1 depth while ponded: %.2f ft (Ymax + Ysur = 15.00 ft)\n",
               yMinPonded);
    if (vMax > 1.0 && yMinPonded < 15.0 - 0.01)
    {
        printf("FAIL: J1 ponds at a depth of %.2f ft, below Ymax + Ysur = 15 ft "
               "(up to %.0f ft3 ponded)\n", yMinPonded, vMax);
        return 1;
    }
    printf("PASS: J1 pressurizes to Ymax + Ysur before any water ponds\n");
    return 0;
}
