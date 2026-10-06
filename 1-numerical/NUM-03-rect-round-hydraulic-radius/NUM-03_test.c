/*
 * NUM-03: RECT_ROUND hydraulic radius inside the round bottom.
 *
 * rect_round_getRofY() returns 0.5*r*(1 - sin(t))/t for a depth inside the
 * circular bottom; a circular segment has R = A/P = 0.5*r*(1 - sin(t)/t).
 * Dynamic wave takes the friction slope from getRofY(), so friction is wrong
 * whenever the water is inside the round bottom.
 *
 * The deck is two 500 ft RECT_ROUND 3 x 4 ft conduits (r = 2 ft) at slope
 * 0.001, n = 0.013, carrying a constant 6.24 cfs to a NORMAL outfall. That is
 * steady uniform flow, so the depth at J1 and J2 must be the normal depth,
 * which this test computes from the exact segment geometry with Manning's
 * equation (A = r^2/2 (t - sin t), P = r t, t = 2 acos(1 - y/r)).
 *
 * Tolerance: 5 % of the normal depth. A correct dynamic-wave solution of
 * uniform flow reproduces the normal depth to well under 1 %; the defect
 * raises the depth by about 60 %.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static const double R = 2.0, N = 0.013, S = 0.001, Q = 6.24;

static double area(double y)
{
    double t = 2.0 * acos(1.0 - y / R);
    return 0.5 * R * R * (t - sin(t));
}

static double manningQ(double y)
{
    double t = 2.0 * acos(1.0 - y / R);
    double a = area(y), p = R * t;
    return 1.486 / N * a * pow(a / p, 2.0 / 3.0) * sqrt(S);
}

int main(void)
{
    double elapsed = 0.0, lo = 1.0e-6, hi = R, yn, d1 = 0, d2 = 0, q2 = 0, v2 = 0;
    int i, err, j1, j2, c2;

    /* normal depth by bisection */
    for (i = 0; i < 100; i++)
    {
        yn = 0.5 * (lo + hi);
        if (manningQ(yn) < Q) lo = yn; else hi = yn;
    }

    err = swmm_open("NUM-03_rect-round.inp", "NUM-03.rpt", "NUM-03.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    j2 = swmm_getIndex(swmm_NODE, "J2");
    c2 = swmm_getIndex(swmm_LINK, "C2");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        d1 = swmm_getValue(swmm_NODE_DEPTH, j1);
        d2 = swmm_getValue(swmm_NODE_DEPTH, j2);
        q2 = swmm_getValue(swmm_LINK_FLOW, c2);
        v2 = swmm_getValue(swmm_LINK_VELOCITY, c2);
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("Steady state after 2 h (Q = %.2f cfs, n = %.3f, S = %.3f, r = %.0f ft)\n", Q, N, S, R);
    printf("                       engine    exact\n");
    printf("Depth at J1 (ft)     %8.3f %8.3f\n", d1, yn);
    printf("Depth at J2 (ft)     %8.3f %8.3f\n", d2, yn);
    printf("C2 flow (cfs)        %8.3f %8.3f\n", q2, Q);
    printf("C2 velocity (ft/s)   %8.3f %8.3f\n", v2, Q / area(yn));

    if (fabs(d1 - yn) > 0.05 * yn || fabs(d2 - yn) > 0.05 * yn)
    {
        printf("FAIL: uniform-flow depth is %.3f ft at J1 and %.3f ft at J2, "
               "normal depth is %.3f ft (%+.0f %%)\n", d1, d2, yn, 100.0 * (d1 / yn - 1.0));
        return 1;
    }
    printf("PASS: uniform-flow depths match the normal depth of the round bottom\n");
    return 0;
}
