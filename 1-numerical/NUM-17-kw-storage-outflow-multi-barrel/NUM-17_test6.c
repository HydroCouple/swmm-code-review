/*
 * NUM-17 for 6.0.0 (C API): under KINWAVE and STEADY
 * routing, the flow from a storage unit into a multi-barrel conduit is that
 * of one barrel.
 *
 * Storage S1 (5000 ft2, 6 ft deep) receives 30 cfs and drains through C1:
 * two 2-ft circular barrels, 400 ft at 1 % slope (n = 0.013), each carrying
 * about 22.6 cfs when full. The decks differ only in FLOW_ROUTING.
 *
 * Correct behaviour, from first principles: at steady state the two barrels
 * carry the 30 cfs inflow, 15 cfs each, and the storage outflow is the
 * conduit's normal flow at the storage depth, so the storage depth is the
 * normal depth of 15 cfs in one barrel (Manning, computed below: about
 * 1.2 ft). Checks at the end of the 12-h run: conduit flow 30 cfs (+/- 1 %),
 * storage depth equal to that normal depth (+/- 0.05 ft), and no flooding.
 * With the bug the conduit passes one barrel's flow (at most 24.4 cfs), the
 * storage fills to 6 ft and floods.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

/* normal depth (ft) of q cfs in a circular pipe of diameter d (Manning) */
static double normalDepth(double q, double d, double n, double s)
{
    double lo = 0.0, hi = 0.9 * d, y = 0.0, th, a, p;
    int i;
    for (i = 0; i < 100; i++)
    {
        y = 0.5 * (lo + hi);
        th = 2.0 * acos(1.0 - 2.0 * y / d);          /* wetted angle */
        a = d * d / 8.0 * (th - sin(th));
        p = d * th / 2.0;
        if (1.486 / n * a * pow(a / p, 2.0 / 3.0) * sqrt(s) < q) lo = y; else hi = y;
    }
    return y;
}

static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *q, double *y, double *maxOver)
{
    double t = 0.0, f;
    int err, s1, c1;
    SWMM_Engine e = swmm_engine_create();

    *maxOver = 0.0;
    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    s1 = swmm_node_index(e, "S1");
    c1 = swmm_link_index(e, "C1");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_link_get_flow(e, c1, q);          /* cfs */
        swmm_node_get_depth(e, s1, y);         /* ft */
        swmm_node_get_overflow(e, s1, &f);     /* cfs */
        if (f > *maxOver) *maxOver = f;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    const char *decks[2] = {"NUM-17_kinwave.inp", "NUM-17_steady.inp"};
    const char *rpts[2]  = {"NUM-17_kw6.rpt", "NUM-17_sf6.rpt"};
    const char *outs[2]  = {"NUM-17_kw6.out", "NUM-17_sf6.out"};
    double yn = normalDepth(15.0, 2.0, 0.013, 0.01);
    double q[2] = {0, 0}, y[2] = {0, 0}, over[2] = {0, 0};
    int i, ok = 1;

    printf("Expected: conduit flow 30 cfs, storage depth %.3f ft (normal depth of 15 cfs "
           "per barrel), no flooding\n", yn);
    printf("%-20s %10s %10s %12s\n", "Deck", "C1 flow", "S1 depth", "S1 flooding");
    printf("%-20s %10s %10s %12s\n", "", "(cfs)", "(ft)", "max (cfs)");
    for (i = 0; i < 2; i++)
    {
        if (runDeck(decks[i], rpts[i], outs[i], &q[i], &y[i], &over[i]))
        {
            printf("FAIL: %s stopped with an error\n", decks[i]);
            return 1;
        }
        printf("%-20s %10.3f %10.3f %12.3f\n", decks[i], q[i], y[i], over[i]);
        if (fabs(q[i] - 30.0) > 0.3 || fabs(y[i] - yn) > 0.05 || over[i] > 0.0) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: the 2-barrel outlet passes %.3f cfs (KINWAVE) and %.3f cfs (STEADY) "
               "instead of 30; the storage is %.3f / %.3f ft deep instead of %.3f and floods at "
               "up to %.3f / %.3f cfs\n", q[0], q[1], y[0], y[1], yn, over[0], over[1]);
        return 1;
    }
    printf("PASS: both barrels carry the storage outflow (30 cfs at the normal depth)\n");
    return 0;
}
