/*
 * CON-10 for 6.0.0: a negative lateral inflow (a withdrawal) removes water
 * from a node but no pollutant mass, so the node's concentration is inflated.
 *
 * 6.0.0's quality loaders add each positive source's own volume to the
 * mixing volume, which is right, but a parity block in
 * QualitySolver::addLoads() then replaces the lateral share with legacy's net
 * lateral flow, and only a NET negative lateral flow books mass leaving.
 *
 * Correct behaviour, as in CON-10_test.c: every source carries 100 mg/L TSS
 * and nothing reacts, so J1 and the outfall must end at 100 mg/L
 * (|C - 100| < 1 mg/L; the bug gives 200) and the quality continuity error
 * must be small (|error| < 1 %).
 * swmm_get_quality_continuity_error() returns a fraction (0.001 = 0.1 %).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

static const char *decks[] = {"CON-10_withdrawal-dw.inp", "CON-10_withdrawal-kw.inp",
                              "CON-10_net-withdrawal-kw.inp"};

int main(void)
{
    int i, rc, nbad = 0;
    printf("Deck                            Routing     J1 TSS   O1 TSS  Quality continuity\n");
    printf("                                            (mg/L)   (mg/L)  error (%%)\n");
    for (i = 0; i < 3; i++)
    {
        double t = 0.0, cJ1 = -1.0, cO1 = -1.0, qualErr = 0.0;
        int j1 = -1, o1 = -1, bad;
        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i], "CON-10_6.rpt", "CON-10_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        if (!rc) { j1 = swmm_node_index(e, "J1"); o1 = swmm_node_index(e, "O1"); }
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
        }
        swmm_node_get_quality(e, j1, 0, &cJ1);
        swmm_node_get_quality(e, o1, 0, &cO1);
        if (!rc) rc = swmm_engine_end(e);
        swmm_get_quality_continuity_error(e, 0, &qualErr);
        qualErr *= 100.0;
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        bad = rc || !(fabs(cJ1 - 100.0) < 1.0) || !(fabs(cO1 - 100.0) < 1.0) ||
              !(fabs(qualErr) < 1.0);
        if (bad) nbad++;
        printf("%-30s  %-8s  %7.2f  %7.2f  %18.3f%s\n", decks[i], i == 0 ? "DYNWAVE" : "KINWAVE",
               cJ1, cO1, qualErr, bad ? "  <-- wrong" : "");
    }
    if (nbad)
    {
        printf("FAIL: in %d of 3 decks the withdrawal leaves its pollutant behind "
               "(concentration above every source's 100 mg/L, or no mass balance)\n", nbad);
        return 1;
    }
    printf("PASS: withdrawals leave at the node's concentration; all nodes stay at 100 mg/L\n");
    return 0;
}
