/*
 * IO-14 for 6.0.0 (same decks and check as IO-14_test.c). 6.0.0 stores any
 * positive code (LinksHandler.cpp) and DynamicWave.cpp takes the culvert
 * branch for code > 0, as legacy does.
 *
 * IO-14: a culvert code above 57 is accepted without a message, and it
 * switches off normal-flow limiting for the conduit.
 *
 * The culvert codes are listed in Table H-1 of the hydraulics reference
 * (Vol. II): 1 to 57. The reader only rejects negative codes. At run time,
 * culvert_getInflow() ignores codes above 57, but dwflow.c has already chosen
 * the culvert branch for any code > 0 and so skips checkNormalFlow().
 *
 * IO-14_code-58.inp and IO-14_code-0.inp are the same model (a steep 3 ft pipe
 * C1 into a mild 4 ft pipe, 30 cfs, NORMAL_FLOW_LIMITED BOTH), with culvert
 * code 58 and with no culvert code on C1.
 *
 * Correct behaviour: code 58 is not a culvert code, so IO-14_code-58.inp is
 * rejected as invalid input. The test also runs IO-14_code-0.inp and prints
 * the depth at J1 at the end of both runs, to show what the accepted code
 * does to the results; that comparison is not the pass criterion.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

/* run a deck; returns the error code and the depth at J1 at the end */
static int run(const char *inp, const char *rpt, const char *out, double *depth)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0;
    int j1, rc = swmm_engine_open(e, inp, rpt, out, NULL);
    *depth = -1.0;
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = rc ? -1 : swmm_node_index(e, "J1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_node_get_depth(e, j1, depth);
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    double d0, d58;
    int err0, err58;

    err58 = run("IO-14_code-58.inp", "IO-14_code-58.rpt", "IO-14_code-58.out", &d58);
    err0 = run("IO-14_code-0.inp", "IO-14_code-0.rpt", "IO-14_code-0.out", &d0);

    printf("Culvert code on C1   error   J1 depth at end (ft)\n");
    printf("  0 (none)           %5d   %8.3f\n", err0, d0);
    if (err58) printf(" 58 (invalid)        %5d   (rejected)\n", err58);
    else       printf(" 58 (invalid)        %5d   %8.3f\n", err58, d58);

    if (err0)
    {
        printf("FAIL: the reference deck without a culvert code did not run (error %d)\n", err0);
        return 1;
    }
    if (!err58)
    {
        printf("FAIL: culvert code 58 (valid codes are 1-57) was accepted without a message; "
               "it turned off normal-flow limiting and changed the J1 depth from %.3f to %.3f ft\n",
               d0, d58);
        return 1;
    }
    printf("PASS: a culvert code outside 1-57 is rejected as invalid input\n");
    return 0;
}
