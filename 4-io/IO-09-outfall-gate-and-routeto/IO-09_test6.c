/*
 * IO-09 for 6.0.0: the same check as IO-09_test.c through the 6.0.0 API.
 * 6.0.0's [OUTFALLS] reader takes the Gated and RouteTo columns one after
 * the other, so it is expected to pass. The legacy description follows.
 *
 * [OUTFALLS] lines are  Name Elev Type (StageData) (Gated) (RouteTo).
 * Legacy outfall_readParams() reads the Gated column only when it is the
 * last token and the RouteTo column only when there is exactly one more, so
 * with both the gate keeps its default, NO.
 *
 * Both decks drain junction J1 (invert 100 ft) through C1 to outfall O1,
 * whose FIXED stage is 102 ft, and give O1 a flap gate:
 *   IO-09_gate.inp          O1 99 FIXED 102 YES
 *   IO-09_gate-routeto.inp  O1 99 FIXED 102 YES S1
 * With the gate nothing can flow back through C1: correct is |C1 flow| = 0
 * and J1 depth = 0 at every routing step, in both decks.
 *
 * Tolerance: 0.001 cfs and 0.001 ft. Without the gate the outfall drives
 * several cfs back into J1 and fills it by feet.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *qMax, double *dMax)
{
    double elapsed = 0.0, q, d;
    int err, j1, c1;
    SWMM_Engine e = swmm_engine_create();

    *qMax = 0.0;
    *dMax = 0.0;
    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    c1 = swmm_link_index(e, "C1");
    while (!err)
    {
        err = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        swmm_link_get_flow(e, c1, &q);
        swmm_node_get_depth(e, j1, &d);
        *qMax = fmax(*qMax, fabs(q));
        *dMax = fmax(*dMax, d);
    }
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    const double tol = 0.001;
    double q1, d1, q2, d2;
    int err1, err2, ok1, ok2;

    err1 = runDeck("IO-09_gate.inp", "IO-09_gate6.rpt", "IO-09_gate6.out", &q1, &d1);
    err2 = runDeck("IO-09_gate-routeto.inp", "IO-09_gate-routeto6.rpt",
                   "IO-09_gate-routeto6.out", &q2, &d2);

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
