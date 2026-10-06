/*
 * NUM-02: a capture node's overflow is shared out among ALL inlets of the
 * model instead of the inlets that drain to it.
 *
 * Two streets each carry 2 cfs to a grate inlet. Inlet S1a drains to sewer
 * node CN1, inlet S2a to sewer node CN2. CN1 also receives 5 cfs of sewer
 * inflow, more than its outlet pipe P1 can carry, so it overflows for the
 * whole run. S1a is the only inlet draining to CN1, so all of CN1's overflow
 * must come back to street 1 as inlet backflow (backflow ratio 1).
 *
 * At steady state (end of the 3-hour run, constant inflows) the backflow is
 * found from two node balances, using only link flows and the overflow:
 *   CN1:  5 + capture  = Q(P1) + overflow     ->  capture
 *   J2 :  Q(S1a) - capture + backflow = Q(S1b) ->  backflow
 * Correct: backflow / overflow = 1. The defect gives area(S1a inlet) / area of
 * all inlets = 0.5 here; the rest of the overflow is removed from the flooding
 * total but sent nowhere, so it also shows up as a continuity error.
 * Tolerance: 10% on the ratio (0.5 vs 1.0 separates the two by 50%).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double elapsed = 0.0, ovf = 0, qS1a = 0, qS1b = 0, qP1 = 0;
    double capture, backflow, ratio;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, cn1, s1a, s1b, p1;

    err = swmm_open("NUM-02_two-capture-nodes.inp", "NUM-02.rpt", "NUM-02.out");
    if (!err) err = swmm_start(1);
    if (err) { printf("FAIL: could not start the run (error %d)\n", err); return 1; }
    cn1 = swmm_getIndex(swmm_NODE, "CN1");
    s1a = swmm_getIndex(swmm_LINK, "S1a");
    s1b = swmm_getIndex(swmm_LINK, "S1b");
    p1  = swmm_getIndex(swmm_LINK, "P1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        ovf  = swmm_getValue(swmm_NODE_OVERFLOW, cn1);
        qS1a = swmm_getValue(swmm_LINK_FLOW, s1a);
        qS1b = swmm_getValue(swmm_LINK_FLOW, s1b);
        qP1  = swmm_getValue(swmm_LINK_FLOW, p1);
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_report();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    capture  = qP1 + ovf - 5.0;
    backflow = qS1b - qS1a + capture;
    ratio    = backflow / ovf;

    printf("At the end of the run (cfs):\n");
    printf("  CN1 overflow                 %8.3f\n", ovf);
    printf("  P1  (CN1 outlet pipe)        %8.3f\n", qP1);
    printf("  S1a (street into inlet)      %8.3f\n", qS1a);
    printf("  S1b (street below inlet)     %8.3f\n", qS1b);
    printf("  capture by inlet S1a         %8.3f\n", capture);
    printf("  backflow into street 1       %8.3f\n", backflow);
    printf("  backflow / CN1 overflow      %8.3f   (correct: 1.000)\n", ratio);
    printf("  routing continuity error     %8.3f %%\n", flowErr);

    if (fabs(ratio - 1.0) > 0.10)
    {
        printf("FAIL: only %.1f%% of CN1's overflow (%.3f of %.3f cfs) returns to the "
               "street of the one inlet draining to it; routing continuity error %.2f%%\n",
               100.0 * ratio, backflow, ovf, flowErr);
        return 1;
    }
    printf("PASS: all of CN1's overflow returns to the street of the inlet draining to it "
           "(ratio %.3f)\n", ratio);
    return 0;
}
