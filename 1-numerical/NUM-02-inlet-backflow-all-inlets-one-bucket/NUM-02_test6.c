/*
 * NUM-02 for 6.0.0: same check as NUM-02_test.c through the 6.0.0 C API.
 *
 * S1a is the only inlet draining to sewer node CN1, which overflows, so all of
 * CN1's overflow must return to street 1 (backflow ratio 1). The backflow is
 * found at steady state from the CN1 and J2 node balances:
 *   capture  = Q(P1) + overflow - 5
 *   backflow = Q(S1b) - Q(S1a) + capture
 * 6.0.0 copies 5.3.0's single-bucket ratios on purpose (Inlet.cpp,
 * computeBackflowRatios), which gives 0.5 here. Tolerance 10% on the ratio.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    double t = 0, ovf = 0, qS1a = 0, qS1b = 0, qP1 = 0, contErr = 0;
    double capture, backflow, ratio;
    int cn1, s1a, s1b, p1;
    SWMM_Engine e = swmm_engine_create();
    int rc = swmm_engine_open(e, "NUM-02_two-capture-nodes.inp", "NUM-02_6.rpt", "NUM-02_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    if (rc) { printf("FAIL: could not start the run (error %d)\n", rc); return 1; }
    cn1 = swmm_node_index(e, "CN1");
    s1a = swmm_link_index(e, "S1a");
    s1b = swmm_link_index(e, "S1b");
    p1  = swmm_link_index(e, "P1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_node_get_overflow(e, cn1, &ovf);
        swmm_link_get_flow(e, s1a, &qS1a);
        swmm_link_get_flow(e, s1b, &qS1b);
        swmm_link_get_flow(e, p1, &qP1);
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_routing_continuity_error(e, &contErr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

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
    printf("  routing continuity error     %8.3f %%\n", 100.0 * contErr);

    if (fabs(ratio - 1.0) > 0.10)
    {
        printf("FAIL: only %.1f%% of CN1's overflow (%.3f of %.3f cfs) returns to the "
               "street of the one inlet draining to it; routing continuity error %.2f%%\n",
               100.0 * ratio, backflow, ovf, 100.0 * contErr);
        return 1;
    }
    printf("PASS: all of CN1's overflow returns to the street of the inlet draining to it "
           "(ratio %.3f)\n", ratio);
    return 0;
}
