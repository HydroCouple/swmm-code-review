/*
 * NUM-10: an ideal pump or DUMMY conduit takes its inlet node's inflow
 * before links with a higher index have added theirs.
 *
 * findLinkFlows() (dynwave.c) resets node inflows each iteration, adds the
 * flows of all true conduits, then visits the other links in link-index
 * order, computing each one's flow and adding it to its nodes at once. An
 * ideal pump and a DUMMY conduit pass on Node[n1].inflow + overflow as it is
 * at that moment, so the flow of a weir, orifice, outlet or pump with a
 * higher index that discharges into n1 is never passed on. It floods.
 *
 * Decks: 5 cfs into J0 flows over weir W1 into J1, which is drained by
 *   NUM-10_ideal-pump.inp     ideal pump P1 ([PUMPS] before [WEIRS])
 *   NUM-10_dummy-conduit.inp  DUMMY conduit D1 ([CONDUITS] before [WEIRS],
 *                             the order the SWMM GUI writes)
 *
 * Correct behaviour: the pass-through link carries all of W1's flow, so in
 * steady state P1/D1 = W1 = 5 cfs and J1 never floods (continuity).
 * Tolerances: flooding below 1% of the inflow volume, and the pass-through
 * flow at 1:00 h within 5% of W1's (with the fix the two differ by ~0.4%
 * from step-to-step oscillation at J1); the defect floods about half the
 * inflow and passes on 2.0-2.5 of 5 cfs.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static int runDeck(const char* inp, const char* rpt, const char* out,
                   const char* passName)
{
    double elapsed = 0.0, tPrev = 0.0, dt;
    double vIn = 0.0, vFlood = 0.0, qW = 0.0, qP = 0.0, yJ1 = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, j0, j1, w1, p1;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    j0 = swmm_getIndex(swmm_NODE, "J0");
    j1 = swmm_getIndex(swmm_NODE, "J1");
    w1 = swmm_getIndex(swmm_LINK, "W1");
    p1 = swmm_getIndex(swmm_LINK, passName);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        dt = (elapsed - tPrev) * 86400.0;
        tPrev = elapsed;
        vIn    += swmm_getValue(swmm_NODE_LATFLOW, j0) * dt;   /* ft3 */
        vFlood += swmm_getValue(swmm_NODE_OVERFLOW, j1) * dt;  /* ft3 */
        if (fabs(elapsed * 24.0 - 1.0) < 1.0e-6)   /* sample at 1:00 h */
        {
            qW  = swmm_getValue(swmm_LINK_FLOW, w1);
            qP  = swmm_getValue(swmm_LINK_FLOW, p1);
            yJ1 = swmm_getValue(swmm_NODE_DEPTH, j1);
        }
    }
    swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    if (err) { printf("%-26s run stopped with error %d\n", inp, err); return 0; }

    printf("%-26s %7.3f %8.3f %8.2f %10.0f %10.0f %7.1f %8.3f\n", inp, qW, qP,
           yJ1, vIn, vFlood, 100.0 * vFlood / vIn, flowErr);
    return vFlood <= 0.01 * vIn && fabs(qP - qW) <= 0.05 * qW;
}

int main(void)
{
    int ok1, ok2;
    printf("At 1:00 h: W1 flow into J1, pass-through link (P1 or D1) flow out of J1;\n"
           "over the run: inflow volume, J1 flooding volume, routing continuity error\n");
    printf("%-26s %7s %8s %8s %10s %10s %7s %8s\n", "deck", "W1", "P1/D1",
           "J1 depth", "inflow", "J1 flood", "flood", "cont.err");
    printf("%-26s %7s %8s %8s %10s %10s %7s %8s\n", "", "(cfs)", "(cfs)",
           "(ft)", "(ft3)", "(ft3)", "(%)", "(%)");
    ok1 = runDeck("NUM-10_ideal-pump.inp", "NUM-10_ideal.rpt",
                  "NUM-10_ideal.out", "P1");
    ok2 = runDeck("NUM-10_dummy-conduit.inp", "NUM-10_dummy.rpt",
                  "NUM-10_dummy.out", "D1");
    if (!ok1 || !ok2)
    {
        printf("FAIL: J1 floods; the weir's flow is not passed on by %s%s%s\n",
               ok1 ? "" : "ideal pump P1", (!ok1 && !ok2) ? " or by " : "",
               ok2 ? "" : "DUMMY conduit D1");
        return 1;
    }
    printf("PASS: the ideal pump and the DUMMY conduit pass on all of the "
           "weir's flow; J1 does not flood\n");
    return 0;
}
