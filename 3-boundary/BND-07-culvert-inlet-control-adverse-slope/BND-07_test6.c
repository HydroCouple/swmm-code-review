/*
 * BND-07 for 6.0.0: the resolver reverses an adverse-slope conduit under
 * dynamic wave, and DWSolver::applyFlowLimits() then applies culvert inlet
 * control in the reversed orientation.
 *
 * Same four decks and checks as BND-07_test.c:
 *  - forward flow (60 cfs entering at J1): J1's steady depth is at least the
 *    HDS-5 submerged inlet-control headwater for a 3 ft code-1 culvert,
 *    4.878 ft (SWMM Hydraulics Reference Manual eq. 7-46), less 0.15 ft, on
 *    both the +0.033% and the -0.033% slope;
 *  - reverse flow (60 cfs entering at J2 and leaving through J1): J2's steady
 *    depth is more than 1 ft below that headwater, because water entering the
 *    barrel at its outlet is not inlet controlled.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

static int runDeck(const char *inp, const char *rpt, const char *out,
                   const char *node, double *depth, double *q)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, v = 0.0;
    int rc, n, c1;

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    n  = swmm_node_index(e, node);
    c1 = swmm_link_index(e, "C1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_node_get_depth(e, n, depth);    /* ft (CFS model) */
        swmm_link_get_flow(e, c1, &v);       /* cfs */
        *q = fabs(v);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    const double Q = 60.0, D = 3.0, c = 0.0398, Y = 0.67;
    const double AD = 3.14159265358979 * D * D / 4.0 * sqrt(D);
    const double hwInlet = D * (c * (Q / AD) * (Q / AD) + Y);
    double hwPos = 0, qPos = 0, hwAdv = 0, qAdv = 0;
    double hrPos = 0, qrPos = 0, hrAdv = 0, qrAdv = 0;
    int err = 0, fwdOk, revOk;

    err |= runDeck("BND-07_positive.inp", "BND-07_positive6.rpt",
                   "BND-07_positive6.out", "J1", &hwPos, &qPos);
    err |= runDeck("BND-07_adverse.inp", "BND-07_adverse6.rpt",
                   "BND-07_adverse6.out", "J1", &hwAdv, &qAdv);
    err |= runDeck("BND-07_reverse-positive.inp", "BND-07_reverse-positive6.rpt",
                   "BND-07_reverse-positive6.out", "J2", &hrPos, &qrPos);
    err |= runDeck("BND-07_reverse-adverse.inp", "BND-07_reverse-adverse6.rpt",
                   "BND-07_reverse-adverse6.out", "J2", &hrAdv, &qrAdv);
    if (err)
    {
        printf("FAIL: a run stopped with an error\n");
        return 1;
    }

    printf("HDS-5 inlet-control headwater for 60 cfs: %.3f ft\n\n", hwInlet);
    printf("Flow      Culvert slope    C1 flow (cfs)   depth where flow enters C1 (ft)\n");
    printf("forward   +0.033%%          %8.2f          J1 %6.3f\n", qPos, hwPos);
    printf("forward   -0.033%%          %8.2f          J1 %6.3f\n", qAdv, hwAdv);
    printf("reverse   +0.033%%          %8.2f          J2 %6.3f\n", qrPos, hrPos);
    printf("reverse   -0.033%%          %8.2f          J2 %6.3f\n", qrAdv, hrAdv);

    /* 0.15 ft: the bug leaves the adverse forward case about 2.5 ft low */
    fwdOk = (hwPos >= hwInlet - 0.15 && hwAdv >= hwInlet - 0.15);
    /* 1 ft: reverse flow without inlet control gives ~2-2.4 ft, the bug 4.88 ft */
    revOk = (hrPos < hwInlet - 1.0 && hrAdv < hwInlet - 1.0);

    if (!fwdOk || !revOk)
    {
        printf("FAIL:");
        if (!fwdOk)
            printf(" forward flow is not inlet controlled (J1 %.3f / %.3f ft on the"
                   " +/- slope, inlet-control headwater %.3f ft)%s", hwPos, hwAdv,
                   hwInlet, revOk ? "" : ";");
        if (!revOk)
            printf(" reverse flow is inlet controlled at the culvert's outlet"
                   " (J2 %.3f / %.3f ft on the +/- slope)", hrPos, hrAdv);
        printf("\n");
        return 1;
    }
    printf("PASS: inlet control applies at the culvert's inlet on both slopes, "
           "and not to reverse flow\n");
    return 0;
}
