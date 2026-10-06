/*
 * IO-09: an outfall's flap gate is ignored when the line also names a RouteTo
 * subcatchment.
 *
 * [OUTFALLS] lines are  Name Elev Type (StageData) (Gated) (RouteTo).
 * outfall_readParams() reads the Gated column only when it is the last token
 * (ntoks == n) and the RouteTo column only when there is exactly one more
 * (ntoks == n+1). With both, the Gated token is never looked at and the gate
 * keeps its default, NO.
 *
 * Both decks drain junction J1 (invert 100 ft) through C1 to outfall O1,
 * whose FIXED stage is 102 ft, and give O1 a flap gate:
 *   IO-09_gate.inp          O1 99 FIXED 102 YES
 *   IO-09_gate-routeto.inp  O1 99 FIXED 102 YES S1
 * With the gate nothing can flow back through C1: correct is |C1 flow| = 0
 * and J1 depth = 0 at every routing step, in both decks. There is no rain,
 * so the RouteTo subcatchment receives nothing either way.
 *
 * Tolerance: 0.001 cfs and 0.001 ft. Without the gate the outfall drives
 * several cfs back into J1 and fills it by feet.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

/* runs a deck; returns the run's error code and the largest |C1 flow| and
   J1 depth seen at any routing step */
static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *qMax, double *dMax)
{
    double elapsed = 0.0;
    int err, j1, c1;

    *qMax = 0.0;
    *dMax = 0.0;
    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    if (!err)
    {
        j1 = swmm_getIndex(swmm_NODE, "J1");
        c1 = swmm_getIndex(swmm_LINK, "C1");
        while (!err)
        {
            err = swmm_step(&elapsed);
            if (elapsed <= 0.0) break;
            *qMax = fmax(*qMax, fabs(swmm_getValue(swmm_LINK_FLOW, c1)));
            *dMax = fmax(*dMax, swmm_getValue(swmm_NODE_DEPTH, j1));
        }
    }
    swmm_end();
    swmm_close();
    return err;
}

int main(void)
{
    const double tol = 0.001;
    double q1, d1, q2, d2;
    int err1, err2, ok1, ok2;

    err1 = runDeck("IO-09_gate.inp", "IO-09_gate.rpt", "IO-09_gate.out", &q1, &d1);
    err2 = runDeck("IO-09_gate-routeto.inp", "IO-09_gate-routeto.rpt",
                   "IO-09_gate-routeto.out", &q2, &d2);

    printf("Outfall O1 (stage 2 ft above J1's invert) with a flap gate:\n");
    printf("  %-26s %6s %16s %16s\n", "[OUTFALLS] line", "error", "max |C1 flow|", "max J1 depth");
    printf("  %-26s %6d %12.3f cfs %13.3f ft\n", "O1 99 FIXED 102 YES", err1, q1, d1);
    printf("  %-26s %6d %12.3f cfs %13.3f ft\n", "O1 99 FIXED 102 YES S1", err2, q2, d2);
    printf("  (correct: 0 cfs and 0 ft for both lines)\n");

    ok1 = (err1 == 0 && q1 < tol && d1 < tol);
    ok2 = (err2 == 0 && q2 < tol && d2 < tol);
    if (err1 || err2)
    {
        printf("FAIL: a run stopped with an error (%d, %d)\n", err1, err2);
        return 1;
    }
    if (!ok1 || !ok2)
    {
        printf("FAIL: the flap gate %s ignored: %.3f cfs flows back through C1 and J1 "
               "fills to %.3f ft\n", ok2 ? "without RouteTo is" : "is",
               ok2 ? q1 : q2, ok2 ? d1 : d2);
        return 1;
    }
    printf("PASS: the flap gate stops backflow with and without a RouteTo subcatchment\n");
    return 0;
}
