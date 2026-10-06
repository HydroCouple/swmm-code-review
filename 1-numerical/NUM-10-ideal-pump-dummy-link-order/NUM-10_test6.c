/*
 * NUM-10 for 6.0.0: the same check as NUM-10_test.c through the 6.0.0 C API.
 *
 * The DW non-conduit callback in SWMMEngine.cpp walks
 * StructureSolver::nc_indices_, which HydStructures.cpp builds in link-index
 * order, computing and scattering each link's flow in turn (legacy parity).
 * An ideal pump or DUMMY conduit with a lower index than the weir feeding
 * its inlet node never sees the weir's flow.
 *
 * Correct behaviour: P1/D1 carry all of W1's 5 cfs and J1 never floods.
 * Tolerances as in NUM-10_test.c (1% of inflow volume, 5% of W1's flow).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

static int runDeck(const char* inp, const char* rpt, const char* out,
                   const char* passName)
{
    double elapsed = 0.0, tPrev = 0.0, dt, v;
    double vIn = 0.0, vFlood = 0.0, qW = 0.0, qP = 0.0, yJ1 = 0.0, contErr = 0.0;
    int rc, j0, j1, w1, p1;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j0 = swmm_node_index(e, "J0");
    j1 = swmm_node_index(e, "J1");
    w1 = swmm_link_index(e, "W1");
    p1 = swmm_link_index(e, passName);
    while (!rc)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        dt = (elapsed - tPrev) * 86400.0;
        tPrev = elapsed;
        swmm_node_get_lateral_inflow(e, j0, &v);  vIn    += v * dt;   /* ft3 */
        swmm_node_get_overflow(e, j1, &v);        vFlood += v * dt;   /* ft3 */
        if (fabs(elapsed * 24.0 - 1.0) < 1.0e-6)   /* sample at 1:00 h */
        {
            swmm_link_get_flow(e, w1, &qW);
            swmm_link_get_flow(e, p1, &qP);
            swmm_node_get_depth(e, j1, &yJ1);
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_get_routing_continuity_error(e, &contErr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("%-26s run stopped with error %d\n", inp, rc); return 0; }

    printf("%-26s %7.3f %8.3f %8.2f %10.0f %10.0f %7.1f %8.3f\n", inp, qW, qP,
           yJ1, vIn, vFlood, 100.0 * vFlood / vIn, 100.0 * contErr);
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
    ok1 = runDeck("NUM-10_ideal-pump.inp", "NUM-10_ideal6.rpt",
                  "NUM-10_ideal6.out", "P1");
    ok2 = runDeck("NUM-10_dummy-conduit.inp", "NUM-10_dummy6.rpt",
                  "NUM-10_dummy6.out", "D1");
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
