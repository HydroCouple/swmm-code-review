/*
 * IO-16 for 6.0.0: the same checks as IO-16_test.c through the 6.0.0 API.
 * 6.0.0 applies property rows after all link sections, so it is expected
 * to pass. The legacy description follows.
 *
 * The input reference allows the sections of an input file in any order, but
 * the legacy reader stores rows as it meets them:
 * A. IO-16_losses-first.inp: [LOSSES] (flap gate on C1) above [CONDUITS].
 *    link_setParams(), run for C1's own row, resets hasFlapGate to 0. The
 *    outfall stage is 2 ft above J1's invert, so with the flap gate nothing
 *    may flow back: correct is |C1 flow| = 0 and J1 depth = 0 at every step.
 * B. IO-16_xsect-above-weirs.inp vs IO-16_xsect-below-weirs.inp: the same
 *    model (C1 with 2 barrels, then weir W1), with [XSECTIONS] above or below
 *    [WEIRS]. W1's row, read before the weir, treats W1 as conduit 0 and sets
 *    its barrels to 1. Correct: both orders give the same results; the test
 *    compares J1's depth and C1's flow at every routing step.
 *
 * Tolerances: 0.001 cfs and 0.001 ft for A (the bug gives several cfs and
 * ft); 1e-6 for B (the two runs do the same arithmetic when the order has no
 * effect; the bug changes J1's depth by feet).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

#define MAXSTEPS 2000

/* runs a deck; records J1 depth and C1 flow at each step; returns error code */
static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *depth, double *flow, int *nSteps)
{
    double elapsed = 0.0;
    int err, j1, c1, n = 0;
    SWMM_Engine e = swmm_engine_create();
    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    c1 = swmm_link_index(e, "C1");
    while (!err)
    {
        err = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        if (n < MAXSTEPS)
        {
            swmm_node_get_depth(e, j1, &depth[n]);
            swmm_link_get_flow(e, c1, &flow[n]);
            n++;
        }
    }
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    *nSteps = n;
    return err;
}

static double maxAbs(const double *a, int n)
{
    double m = 0.0;
    int i;
    for (i = 0; i < n; i++) m = fmax(m, fabs(a[i]));
    return m;
}

static double d1[MAXSTEPS], q1[MAXSTEPS], d2[MAXSTEPS], q2[MAXSTEPS];

int main(void)
{
    int err, n1 = 0, n2 = 0, i, okA, okB;
    double qMax, dMax, dDiff = 0.0, qDiff = 0.0;

    /* ---- A: flap gate in [LOSSES] above [CONDUITS] */
    err = runDeck("IO-16_losses-first.inp", "IO-16_losses6.rpt", "IO-16_losses6.out", d1, q1, &n1);
    qMax = maxAbs(q1, n1);
    dMax = maxAbs(d1, n1);
    printf("A. [LOSSES] above [CONDUITS], flap gate on C1 (run returned %d)\n", err);
    printf("   largest |C1 flow| %.3f cfs, largest J1 depth %.3f ft (both should be 0)\n\n",
           qMax, dMax);
    okA = (err == 0 && qMax <= 0.001 && dMax <= 0.001);

    /* ---- B: [XSECTIONS] above or below [WEIRS] */
    err = runDeck("IO-16_xsect-below-weirs.inp", "IO-16_below6.rpt", "IO-16_below6.out", d1, q1, &n1);
    if (!err) err = runDeck("IO-16_xsect-above-weirs.inp", "IO-16_above6.rpt", "IO-16_above6.out", d2, q2, &n2);
    printf("B. the same model with [XSECTIONS] below or above [WEIRS] (runs returned %d)\n", err);
    printf("                          steps   J1 max depth (ft)   C1 max flow (cfs)\n");
    printf("   [XSECTIONS] below      %5d   %17.3f   %17.3f\n", n1, maxAbs(d1, n1), maxAbs(q1, n1));
    printf("   [XSECTIONS] above      %5d   %17.3f   %17.3f\n", n2, maxAbs(d2, n2), maxAbs(q2, n2));
    for (i = 0; i < n1 && i < n2; i++)
    {
        dDiff = fmax(dDiff, fabs(d1[i] - d2[i]));
        qDiff = fmax(qDiff, fabs(q1[i] - q2[i]));
    }
    printf("   largest difference at the same step: J1 depth %.6f ft, C1 flow %.6f cfs\n",
           dDiff, qDiff);
    okB = (err == 0 && n1 == n2 && n1 > 0 && dDiff <= 1e-6 && qDiff <= 1e-6);

    if (!okA || !okB)
    {
        printf("FAIL: section order changes the results:%s%s\n",
               okA ? "" : " the flap gate in [LOSSES] is lost (reverse flow through C1);",
               okB ? "" : " [XSECTIONS] above [WEIRS] changes J1's depth (C1 loses a barrel)");
        return 1;
    }
    printf("PASS: the flap gate holds and the results do not depend on the section order\n");
    return 0;
}
