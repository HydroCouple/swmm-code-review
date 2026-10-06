/*
 * NUM-07: the full-flow section factor of a FORCE_MAIN is A*R^0.63 (the
 * Hazen-Williams radius exponent), but every use of it multiplies by a
 * Manning factor 1.486*sqrt(S)/n.
 *
 * Correct behaviour, from the SWMM Hydraulics Reference Manual:
 *  - a conduit's full flow is the Manning full normal flow
 *    Qfull = 1.486/n * A * R^(2/3) * sqrt(S) (eq. 3-23 at full depth);
 *  - under dynamic wave a Hazen-Williams force main uses the equivalent
 *    n = 1.067/C * (D/S)^0.04 (eq. 7-35), chosen so that this Manning full flow
 *    equals the Hazen-Williams full flow (eq. 7-34);
 *  - under kinematic wave the Manning equation with the n of [CONDUITS] is
 *    used, so a 1 ft FORCE_MAIN carries the same full flow as a 1 ft CIRCULAR.
 *
 * Part A (NUM-07_dw-hw.inp, dynamic wave): full flow of force mains of 1, 0.5
 * and 4 ft (C = 130, S = 0.1%) against the Manning full flow with the
 * equivalent n. Using R^0.63 instead of R^(2/3) inflates it by R^-0.037:
 * +5% at 1 ft, +8% at 0.5 ft, nothing at 4 ft (R = 1).
 * Part B (NUM-07_kw.inp, kinematic wave): 2 cfs, more than either pipe can
 * carry, enters a 1 ft FORCE_MAIN and a 1 ft CIRCULAR pipe (n = 0.013,
 * S = 0.1%). The flow each passes at the end of the run must be the Manning
 * full flow, 1.127 cfs.
 *
 * Tolerance 1%: the fixed engines match to 0.1%, the bug is 5-8% off.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#define PI 3.14159265358979

static double manningFull(double d, double n, double s)
{
    double a = PI / 4.0 * d * d, r = d / 4.0;
    return 1.486 / n * a * pow(r, 2.0 / 3.0) * sqrt(s);
}

static double hazenWilliamsFull(double d, double c, double s)
{
    double a = PI / 4.0 * d * d, r = d / 4.0;
    return 1.318 * c * a * pow(r, 0.63) * pow(s, 0.54);
}

int main(void)
{
    const char *fmName[3] = {"FM1", "FM05", "FM4"};
    const double fmD[3] = {1.0, 0.5, 4.0};
    const double C = 130.0, S = 1.0 / sqrt(1000.0 * 1000.0 - 1.0);  /* 1 ft drop, 1000 ft */
    double qFull[3], qExp, nEq, err, worst = 0.0;
    double elapsed = 0.0, qFm = 0.0, qCirc = 0.0, qKw;
    int i, rc, fm, circ;

    /* ---- Part A: dynamic wave, Hazen-Williams force mains */
    rc = swmm_open("NUM-07_dw-hw.inp", "NUM-07_dw-hw.rpt", "NUM-07_dw-hw.out");
    if (!rc) rc = swmm_start(1);
    if (rc) { printf("FAIL: could not start the dynamic wave deck (error %d)\n", rc); return 1; }
    for (i = 0; i < 3; i++)
        qFull[i] = swmm_getValue(swmm_LINK_FULLFLOW, swmm_getIndex(swmm_LINK, (char *)fmName[i]));
    while (!rc)
    {
        rc = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    if (rc) { printf("FAIL: the dynamic wave run stopped with error %d\n", rc); return 1; }
    elapsed = 0.0;

    printf("Dynamic wave, H-W force mains (C = 130, S = 0.1%%)\n");
    printf("Link   D (ft)   equiv. n   Manning full   H-W full   SWMM full   error\n");
    for (i = 0; i < 3; i++)
    {
        nEq = 1.067 / C * pow(fmD[i] / S, 0.04);
        qExp = manningFull(fmD[i], nEq, S);
        err = qFull[i] / qExp - 1.0;
        if (fabs(err) > fabs(worst)) worst = err;
        printf("%-5s  %5.2f    %7.5f   %9.4f    %9.4f  %9.4f   %+6.2f%%\n", fmName[i], fmD[i],
               nEq, qExp, hazenWilliamsFull(fmD[i], C, S), qFull[i], 100.0 * err);
    }

    /* ---- Part B: kinematic wave, FORCE_MAIN vs CIRCULAR, both overfed */
    rc = swmm_open("NUM-07_kw.inp", "NUM-07_kw.rpt", "NUM-07_kw.out");
    if (!rc) rc = swmm_start(1);
    fm = swmm_getIndex(swmm_LINK, "FM1");
    circ = swmm_getIndex(swmm_LINK, "CIRC1");
    while (!rc)
    {
        rc = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        qFm = swmm_getValue(swmm_LINK_FLOW, fm);
        qCirc = swmm_getValue(swmm_LINK_FLOW, circ);
    }
    swmm_end();
    swmm_close();
    if (rc) { printf("FAIL: the kinematic wave run stopped with error %d\n", rc); return 1; }

    qKw = manningFull(1.0, 0.013, S);
    printf("\nKinematic wave, 2 cfs into 1 ft pipes (n = 0.013, S = 0.1%%)\n");
    printf("Manning full flow                 %7.4f cfs\n", qKw);
    printf("CIRCULAR   flow at end of run     %7.4f cfs  (%+.2f%%)\n", qCirc, 100.0 * (qCirc / qKw - 1.0));
    printf("FORCE_MAIN flow at end of run     %7.4f cfs  (%+.2f%%)\n", qFm, 100.0 * (qFm / qKw - 1.0));

    /* 1%: the fixed full flows are within 0.1%, the bug inflates them by 5-8% */
    if (fabs(worst) > 0.01 || fabs(qFm / qKw - 1.0) > 0.01 || fabs(qCirc / qKw - 1.0) > 0.01)
    {
        printf("FAIL: force-main full flow is not the Manning full flow: dynamic wave up to "
               "%+.1f%%, kinematic wave %.4f cfs instead of %.4f cfs (%+.1f%%)\n",
               100.0 * worst, qFm, qKw, 100.0 * (qFm / qKw - 1.0));
        return 1;
    }
    printf("PASS: force-main full flow is the Manning full flow under dynamic and kinematic wave\n");
    return 0;
}
