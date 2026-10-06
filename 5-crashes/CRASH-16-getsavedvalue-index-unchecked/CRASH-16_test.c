/*
 * CRASH-16: swmm_getSavedValue never checks the object index.
 *
 * swmm_getSavedValue(property, index, period) checks that a project is open,
 * that the run has ended and that period is in 1..Nperiods, but not index.
 * getSavedSubcatchValue / getSavedNodeValue / getSavedLinkValue then read
 * Subcatch[index].rptFlag, Node[index].rptFlag or Link[index].rptFlag, so an
 * index one past the end (or -1, the "not found" value of swmm_getIndex)
 * reads outside the object array, and the garbage is used as a record
 * offset into the binary output file.
 *
 * Correct behaviour: an index outside 0..count-1 gives 0, the value the
 * function already returns for an invalid period, without touching memory
 * outside the arrays. An index in range still returns the saved result
 * (J1 receives runoff from 1 in/hr of rain, so its depth at the first
 * reporting period, 0:15, is > 0). All calls use period 1.
 * Under AddressSanitizer the out-of-bounds read stops the test (CRASH).
 */
#include <stdio.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, v;
    int err, i, bad = 0;
    int nSub, nNode, nLink, j1;
    struct { const char *what; int prop; int index; } badCase[6];

    err = swmm_open("CRASH-16_model.inp", "CRASH-16.rpt", "CRASH-16.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    nSub  = swmm_getCount(swmm_SUBCATCH);
    nNode = swmm_getCount(swmm_NODE);
    nLink = swmm_getCount(swmm_LINK);
    j1    = swmm_getIndex(swmm_NODE, "J1");

    v = swmm_getSavedValue(swmm_NODE_DEPTH, j1, 1);
    printf("saved depth of J1 (index %d), period 1 (0:15) = %.4f ft\n", j1, v);
    if (!(v > 0.0)) bad++;

    badCase[0].what = "node index = node count";       badCase[0].prop = swmm_NODE_DEPTH;      badCase[0].index = nNode;
    badCase[1].what = "node index = -1";               badCase[1].prop = swmm_NODE_DEPTH;      badCase[1].index = -1;
    badCase[2].what = "link index = link count";       badCase[2].prop = swmm_LINK_FLOW;       badCase[2].index = nLink;
    badCase[3].what = "link index = -1";               badCase[3].prop = swmm_LINK_FLOW;       badCase[3].index = -1;
    badCase[4].what = "subcatch index = subcatch count"; badCase[4].prop = swmm_SUBCATCH_RUNOFF; badCase[4].index = nSub;
    badCase[5].what = "subcatch index = -1";           badCase[5].prop = swmm_SUBCATCH_RUNOFF; badCase[5].index = -1;

    for (i = 0; i < 6; i++)
    {
        printf("swmm_getSavedValue, %-32s (%2d) ...\n", badCase[i].what, badCase[i].index);
        fflush(stdout);
        v = swmm_getSavedValue(badCase[i].prop, badCase[i].index, 1);
        printf("    returned %g\n", v);
        if (v != 0.0) bad++;
    }
    swmm_close();

    if (bad)
    {
        printf("FAIL: %d calls returned a value for an object that does not exist "
               "(or no value for one that does)\n", bad);
        return 1;
    }
    printf("PASS: out-of-range indices return 0 and in-range ones the saved result\n");
    return 0;
}
