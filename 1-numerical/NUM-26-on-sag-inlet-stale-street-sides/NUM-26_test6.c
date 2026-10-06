/*
 * NUM-26 for 6.0.0: same check as NUM-26_test.c through the 6.0.0 C API.
 * 6.0.0 reproduces the stale street sides on purpose (Inlet.hpp nsides_prev_).
 *
 * Street H is a half street (Sides = 1) with one 2 ft x 2 ft grate placed
 * ON_SAG. Street F, unconnected to H, is a full street (Sides = 2) with the
 * same grate placed ON_GRADE. One grate on a one-sided street must capture
 * the flow of one grate at the depth it sees.
 *
 * Expected capture of one grate on sag (HEC-22 3rd ed. Eq. 4-26 / 4-27, with
 * SWMM's curb-side conventions, no gutter depression so Sw = Sx = 0.02):
 *   wetted grate width  Wg' = min(Wg, d / Sw)
 *   average depth       di  = d - Wg'/2 * Sw
 *   perimeter           P   = Lg + 2 Wg'          (curb side not counted)
 *   open area           Ao  = Lg Wg' * 0.5        (GENERIC, 50% open)
 *   weir    (d <= 1.79 Ao/P): Q = 3.0 P di^1.5
 *   orifice (otherwise):      Q = 0.67 Ao sqrt(2 g di)
 * d is the depth at the inlet's bypass node BH. Measured capture is the drop
 * in street flow across the inlet, Q(H1) - Q(H2), at the end of the 3-hour
 * run with constant inflows (steady state).
 * Tolerance: 10% on measured / expected; the defect gives 2.0.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

static double sagGrate(double d)
{
    const double Lg = 2.0, Wg = 2.0, Sw = 0.02, open = 0.5;
    double w = Wg, di, P, Ao;
    if (d <= Wg * Sw) w = d / Sw;
    di = d - 0.5 * w * Sw;
    P  = Lg + 2.0 * w;
    Ao = Lg * w * open;
    if (d <= 1.79 * Ao / P) return 3.0 * P * pow(di, 1.5);
    return 0.67 * Ao * sqrt(2.0 * 32.16 * di);
}

int main(void)
{
    double t = 0.0, d = 0, q1 = 0, q2 = 0, got, want, ratio;
    int err, bh, h1, h2;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "NUM-26_half-and-full-street.inp", "NUM-26_6.rpt", "NUM-26_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (err) { printf("FAIL: could not start the run (error %d)\n", err); return 1; }
    bh = swmm_node_index(e, "BH");
    h1 = swmm_link_index(e, "H1");
    h2 = swmm_link_index(e, "H2");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_node_get_depth(e, bh, &d);
        swmm_link_get_flow(e, h1, &q1);
        swmm_link_get_flow(e, h2, &q2);
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    got = q1 - q2;
    want = sagGrate(d);
    ratio = got / want;
    printf("Half street H, one grate on sag (end of run):\n");
    printf("  depth at inlet BH          %8.4f ft\n", d);
    printf("  street flow H1 / H2        %8.3f / %.3f cfs\n", q1, q2);
    printf("  captured flow              %8.3f cfs\n", got);
    printf("  one grate at that depth    %8.3f cfs (HEC-22)\n", want);
    printf("  captured / one grate       %8.3f   (correct: 1.000)\n", ratio);
    if (fabs(ratio - 1.0) > 0.10)
    {
        printf("FAIL: the one-sided sag inlet captures %.3f cfs, %.2f times what one grate "
               "takes at %.3f ft (%.3f cfs)\n", got, ratio, d, want);
        return 1;
    }
    printf("PASS: the one-sided sag inlet captures what one grate takes at its depth "
           "(ratio %.3f)\n", ratio);
    return 0;
}
