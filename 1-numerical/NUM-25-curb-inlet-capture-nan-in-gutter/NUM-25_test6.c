/*
 * NUM-25 for 6.0.0: same check as NUM-25_test.c through the 6.0.0 C API.
 * Inlet.cpp getCurbInletCapture() has the same unguarded getEo(T - W) call.
 *
 * Four identical streets (HEC-22 "Street2": Sx = 0.02, gutter depression
 * a = 0.0833 ft, gutter width W = 2 ft, n = 0.016, longitudinal slope 0.01)
 * carry 0.05, 0.08, 0.10 and 0.50 cfs to a 1-ft curb opening inlet.
 *
 * Expected capture, HEC-22 (3rd ed.) section 4.4.4, computed here
 * independently of SWMM:
 *   spread T: inside the gutter (T <= W) all flow is in the gutter, so
 *             Eo = 1 and Q = (0.56/n) SL^0.5 Sw^1.67 T^2.67, Sw = Sx + a/W;
 *             beyond the gutter, solve Q (1 - Eo(T)) = (0.56/n) SL^0.5
 *             Sx^1.67 (T - W)^2.67 with Eo from Eq. 4-4.
 *   Se = Sx + (a/W) Eo                                 Eq. 4-24
 *   Lt = 0.6 Q^0.42 SL^0.3 (1/(n Se))^0.6              Eq. 4-22a
 *   E  = 1 - (1 - L/Lt)^1.8  (L < Lt)                  Eq. 4-23
 * Measured capture = Q(Sxa) - Q(Sxb), the drop in street flow across the
 * inlet, at the end of the 3-hour run with constant inflows.
 * Tolerance: 2 percentage points (routing and SWMM's 0.01-ft spread
 * iteration); the defect gives 100% against 44-56%.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

#define NS 4
static const double Sx = 0.02, a = 0.0833, W = 2.0, n = 0.016, SL = 0.01, L = 1.0;

static double eo(double T)                 /* HEC-22 Eq. 4-4, T > W */
{
    double Sr = (Sx + a / W) / Sx;
    return 1.0 / (1.0 + Sr / (pow(1.0 + Sr / (T / W - 1.0), 2.67) - 1.0));
}

static double hec22Capture(double Q)       /* percent of Q */
{
    double Sw = Sx + a / W, Eo = 1.0, T, lo, hi, Se, Lt, E = 1.0;
    int i;
    T = pow(Q / (0.56 / n * sqrt(SL) * pow(Sw, 1.67)), 0.375);
    if (T > W)
    {
        lo = W + 1e-9; hi = 20.0;
        for (i = 0; i < 100; i++)
        {
            T = 0.5 * (lo + hi);
            if (0.56 / n * sqrt(SL) * pow(Sx, 1.67) * pow(T - W, 2.67) > (1.0 - eo(T)) * Q) hi = T;
            else lo = T;
        }
        Eo = eo(T);
    }
    Se = Sx + (a / W) * Eo;
    Lt = 0.6 * pow(Q, 0.42) * pow(SL, 0.3) * pow(1.0 / (n * Se), 0.6);
    if (L < Lt) E = 1.0 - pow(1.0 - L / Lt, 1.8);
    return 100.0 * E;
}

int main(void)
{
    const char *up[NS] = {"S1a", "S2a", "S3a", "S4a"}, *dn[NS] = {"S1b", "S2b", "S3b", "S4b"};
    int iu[NS], id[NS], k, err, bad = 0;
    double t = 0.0, qu[NS] = {0}, qd[NS] = {0}, got, want, worst = 0.0, worstQ = 0, worstGot = 0, worstWant = 0;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "NUM-25_curb-inlet-low-flows.inp", "NUM-25_6.rpt", "NUM-25_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (err) { printf("FAIL: could not start the run (error %d)\n", err); return 1; }
    for (k = 0; k < NS; k++)
    {
        iu[k] = swmm_link_index(e, up[k]);
        id[k] = swmm_link_index(e, dn[k]);
    }
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        for (k = 0; k < NS; k++)
        {
            swmm_link_get_flow(e, iu[k], &qu[k]);
            swmm_link_get_flow(e, id[k], &qd[k]);
        }
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    printf("Street  Q (cfs)  bypass (cfs)  capture %%  HEC-22 %%\n");
    for (k = 0; k < NS; k++)
    {
        got = 100.0 * (qu[k] - qd[k]) / qu[k];
        want = hec22Capture(qu[k]);
        printf("%-6s  %7.3f  %12.4f  %9.2f  %8.2f\n", up[k], qu[k], qd[k], got, want);
        if (fabs(got - want) > 2.0) bad++;
        if (fabs(got - want) > worst)
        {
            worst = fabs(got - want); worstQ = qu[k]; worstGot = got; worstWant = want;
        }
    }
    if (bad)
    {
        printf("FAIL: %d of %d curb inlets differ from HEC-22 by more than 2 points "
               "(worst: %.3f cfs captured at %.2f%%, HEC-22 %.2f%%)\n", bad, NS, worstQ, worstGot, worstWant);
        return 1;
    }
    printf("PASS: curb inlet capture follows HEC-22 inside and beyond the depressed gutter "
           "(largest difference %.2f points)\n", worst);
    return 0;
}
