/*
 * BND-07: dynamic wave reverses a conduit with an adverse slope, and culvert
 * inlet control then follows the reversed orientation: it no longer applies
 * to flow entering the culvert's inlet, and applies instead to flow entering
 * its outlet.
 *
 * C1 is a 3 ft circular concrete culvert, code 1 (square edge with headwall),
 * 60 ft long, from J1 (inlet, invert 100.00) to J2. Each pair of decks differs
 * only in J2's invert: 99.98 (slope +0.033%) or 100.02 (adverse, -0.033%).
 *
 * Forward flow (BND-07_positive.inp, BND-07_adverse.inp): a steady 60 cfs
 * enters at J1; J2 drains through a 10 ft drop to a free outfall, so the
 * culvert is not under outlet control. By the SWMM Hydraulics Reference
 * Manual (section 7.4, eq. 7-46; FHWA HDS-5) the headwater at the inlet node
 * is then the inlet-control headwater. Q/(A*sqrt(D)) = 60/(7.069*sqrt(3)) =
 * 4.90 > 4, so the submerged form applies:
 *     HW/D = c*(Q/(A*sqrt(D)))^2 + Y - 0.5*S,  c = 0.0398, Y = 0.67
 * which gives HW = 4.878 ft (the 0.5*S term is 0.0005 ft for |S| = 0.033%).
 * Check: J1's steady depth is at least HW - 0.15 ft on both slopes. With the
 * bug the adverse culvert gives about 2.4 ft, 2.5 ft below.
 *
 * Reverse flow (BND-07_reverse-positive.inp, BND-07_reverse-adverse.inp):
 * 60 cfs enters at J2 and flows back through C1 to J1, which drains freely.
 * The water enters the barrel at its outlet, where there is no culvert inlet,
 * so inlet control does not apply (the manual evaluates it at the inlet node)
 * and J2's head is set by the barrel and J1's drain: about 2-2.4 ft here. With
 * the bug the reversed adverse culvert treats J2 as its inlet and puts J2 on
 * the inlet-control curve, 4.88 ft. Check: J2's steady depth is more than
 * 1 ft below HW (3.88 ft), which leaves about 1.5 ft of margin on either side.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static int runDeck(const char *inp, const char *rpt, const char *out,
                   const char *node, double *depth, double *q)
{
    double elapsed = 0.0;
    int err, n, c1;

    err = swmm_open((char *)inp, (char *)rpt, (char *)out);
    if (!err) err = swmm_start(1);
    if (err) { swmm_close(); return err; }
    n  = swmm_getIndex(swmm_NODE, (char *)node);
    c1 = swmm_getIndex(swmm_LINK, "C1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        *depth = swmm_getValue(swmm_NODE_DEPTH, n);
        *q = fabs(swmm_getValue(swmm_LINK_FLOW, c1));
    }
    swmm_end();
    swmm_close();
    return err;
}

int main(void)
{
    const double Q = 60.0, D = 3.0, c = 0.0398, Y = 0.67;
    const double AD = 3.14159265358979 * D * D / 4.0 * sqrt(D);
    const double hwInlet = D * (c * (Q / AD) * (Q / AD) + Y);
    double hwPos = 0, qPos = 0, hwAdv = 0, qAdv = 0;
    double hrPos = 0, qrPos = 0, hrAdv = 0, qrAdv = 0;
    int err = 0, fwdOk, revOk;

    err |= runDeck("BND-07_positive.inp", "BND-07_positive.rpt",
                   "BND-07_positive.out", "J1", &hwPos, &qPos);
    err |= runDeck("BND-07_adverse.inp", "BND-07_adverse.rpt",
                   "BND-07_adverse.out", "J1", &hwAdv, &qAdv);
    err |= runDeck("BND-07_reverse-positive.inp", "BND-07_reverse-positive.rpt",
                   "BND-07_reverse-positive.out", "J2", &hrPos, &qrPos);
    err |= runDeck("BND-07_reverse-adverse.inp", "BND-07_reverse-adverse.rpt",
                   "BND-07_reverse-adverse.out", "J2", &hrAdv, &qrAdv);
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
