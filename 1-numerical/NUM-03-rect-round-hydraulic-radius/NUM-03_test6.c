/*
 * NUM-03 for 6.0.0: RECT_ROUND hydraulic radius inside the round bottom.
 *
 * Same check as NUM-03_test.c: two 500 ft RECT_ROUND 3 x 4 ft conduits
 * (r = 2 ft), slope 0.001, n = 0.013, constant 6.24 cfs into a NORMAL outfall.
 * Steady uniform flow, so the depths at J1 and J2 must equal the normal depth
 * computed from the exact circular-segment geometry. Tolerance 5 % (a correct
 * solution is within 1 %; the defect gives about +60 %).
 *
 * 6.0.0 also exposes the section geometry directly, so the test prints
 * swmm_xsect_hydrad_of_depth() for the same section next to the exact A/P,
 * and requires them to agree to 1 % (the defect is off by up to 100x).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_xsect.h"

static const double R = 2.0, N = 0.013, S = 0.001, Q = 6.24;

static double theta(double y) { return 2.0 * acos(1.0 - y / R); }
static double area(double y) { double t = theta(y); return 0.5 * R * R * (t - sin(t)); }
static double hydrad(double y) { return area(y) / (R * theta(y)); }

static double manningQ(double y)
{
    double a = area(y);
    return 1.486 / N * a * pow(hydrad(y), 2.0 / 3.0) * sqrt(S);
}

int main(void)
{
    double t = 0.0, lo = 1.0e-6, hi = R, yn = 0, d1 = 0, d2 = 0, q2 = 0, v2 = 0;
    double ys[] = {0.25, 0.5, 1.0, 1.5, 1.9}, worst = 0.0;
    int i, rc, j1, j2, c2, ok = 1;
    SWMM_XSect xs = NULL;
    SWMM_Engine e;

    for (i = 0; i < 100; i++)
    {
        yn = 0.5 * (lo + hi);
        if (manningQ(yn) < Q) lo = yn; else hi = yn;
    }

    /* --- the section's own hydraulic radius */
    printf("RECT_ROUND 3 x 4 ft, r = 2 ft: hydraulic radius in the round bottom\n");
    printf("  y (ft)   engine R   exact A/P   ratio\n");
    rc = swmm_xsect_create(SWMM_XSECT_RECT_ROUND, 3.0, 4.0, 0.0, 0.0, SWMM_UNITS_US, &xs);
    for (i = 0; !rc && i < 5; i++)
    {
        double r = 0.0;
        rc = swmm_xsect_hydrad_of_depth(xs, ys[i], &r);
        printf("  %6.2f   %8.4f   %9.4f   %5.3f\n", ys[i], r, hydrad(ys[i]), r / hydrad(ys[i]));
        if (fabs(r / hydrad(ys[i]) - 1.0) > worst) worst = fabs(r / hydrad(ys[i]) - 1.0);
    }
    if (xs) swmm_xsect_free(xs);
    if (rc) { printf("FAIL: cross-section API returned %d\n", rc); return 1; }
    if (worst > 0.01) ok = 0;

    /* --- the dynamic-wave run */
    e = swmm_engine_create();
    rc = swmm_engine_open(e, "NUM-03_rect-round.inp", "NUM-03_6.rpt", "NUM-03_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    j2 = swmm_node_index(e, "J2");
    c2 = swmm_link_index(e, "C2");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_node_get_depth(e, j1, &d1);
        swmm_node_get_depth(e, j2, &d2);
        swmm_link_get_flow(e, c2, &q2);
        swmm_link_get_velocity(e, c2, &v2);
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

    printf("\nSteady state after 2 h (Q = %.2f cfs, n = %.3f, S = %.3f, r = %.0f ft)\n", Q, N, S, R);
    printf("                       engine    exact\n");
    printf("Depth at J1 (ft)     %8.3f %8.3f\n", d1, yn);
    printf("Depth at J2 (ft)     %8.3f %8.3f\n", d2, yn);
    printf("C2 flow (cfs)        %8.3f %8.3f\n", q2, Q);
    printf("C2 velocity (ft/s)   %8.3f %8.3f\n", v2, Q / area(yn));
    if (fabs(d1 - yn) > 0.05 * yn || fabs(d2 - yn) > 0.05 * yn) ok = 0;

    if (!ok)
    {
        printf("FAIL: R(y) off by up to %.0f %%; uniform-flow depth %.3f ft at J1, "
               "normal depth %.3f ft (%+.0f %%)\n", 100.0 * worst, d1, yn, 100.0 * (d1 / yn - 1.0));
        return 1;
    }
    printf("PASS: R(y) is A/P and uniform-flow depths match the normal depth\n");
    return 0;
}
