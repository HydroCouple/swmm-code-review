/*
 * NUM-07 for 6.0.0: XSection.cpp gives a FORCE_MAIN the full-flow section
 * factor A*R^0.63, while every use of it multiplies by a Manning factor
 * 1.486*sqrt(S)/n.
 *
 * Same decks and checks as NUM-07_test.c (see there for the reasoning):
 *  - Part A, dynamic wave: the full flow of H-W force mains of 1, 0.5 and 4 ft
 *    must be the Manning full flow with the equivalent n = 1.067/C*(D/S)^0.04
 *    (Hydraulics Reference Manual eqs. 7-34, 7-35). The 6.0.0 API has no
 *    full-flow getter; swmm_link_get_capacities_bulk() returns q/q_full, so
 *    q_full = q / (q/q_full) at the end of the run (0.1 cfs in each main).
 *  - Part B, kinematic wave: 2 cfs into a 1 ft FORCE_MAIN and a 1 ft CIRCULAR
 *    pipe (n = 0.013, S = 0.1%); each must pass the Manning full flow.
 * Tolerance 1%: the fixed engines match to 0.1%, the bug is 5-8% off.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

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

/* runs a deck to the end and leaves each link's flow and q/q_full in q[], cap[] */
static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *q, double *cap, int nmax)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0;
    int rc, n;

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    n = swmm_link_count(e);
    if (n > nmax) n = nmax;
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_link_get_flows_bulk(e, q, n);        /* cfs (CFS model) */
        swmm_link_get_capacities_bulk(e, cap, n); /* q / q_full */
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    const char *fmName[3] = {"FM1", "FM05", "FM4"};
    const double fmD[3] = {1.0, 0.5, 4.0};
    const double C = 130.0, S = 1.0 / sqrt(1000.0 * 1000.0 - 1.0);  /* 1 ft drop, 1000 ft */
    double q[8] = {0}, cap[8] = {0}, qFull, qExp, nEq, err, worst = 0.0;
    double qFm, qCirc, qKw;
    int i, rc;

    /* ---- Part A: dynamic wave, Hazen-Williams force mains (links in deck order) */
    rc = runDeck("NUM-07_dw-hw.inp", "NUM-07_dw-hw6.rpt", "NUM-07_dw-hw6.out", q, cap, 8);
    if (rc) { printf("FAIL: the dynamic wave run stopped with error %d\n", rc); return 1; }

    printf("Dynamic wave, H-W force mains (C = 130, S = 0.1%%)\n");
    printf("Link   D (ft)   equiv. n   Manning full   H-W full   SWMM full   error\n");
    for (i = 0; i < 3; i++)
    {
        nEq = 1.067 / C * pow(fmD[i] / S, 0.04);
        qExp = manningFull(fmD[i], nEq, S);
        qFull = (cap[i] > 0.0) ? q[i] / cap[i] : 0.0;
        err = qFull / qExp - 1.0;
        if (fabs(err) > fabs(worst)) worst = err;
        printf("%-5s  %5.2f    %7.5f   %9.4f    %9.4f  %9.4f   %+6.2f%%\n", fmName[i], fmD[i],
               nEq, qExp, hazenWilliamsFull(fmD[i], C, S), qFull, 100.0 * err);
    }

    /* ---- Part B: kinematic wave, FORCE_MAIN (link 0) vs CIRCULAR (link 1) */
    rc = runDeck("NUM-07_kw.inp", "NUM-07_kw6.rpt", "NUM-07_kw6.out", q, cap, 8);
    if (rc) { printf("FAIL: the kinematic wave run stopped with error %d\n", rc); return 1; }
    qFm = q[0];
    qCirc = q[1];

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
