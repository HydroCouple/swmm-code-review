/*
 * API-10 for 6.0.0: a node invert or link offset set through the API after
 * swmm_engine_open() must update the conduit slope and the flows that depend
 * on it.
 *
 * 6.0.0 computes the conduit slope in PostParseResolver during
 * swmm_engine_open() and derives beta and the full flow from it there.
 * swmm_node_set_invert_elev() and swmm_link_set_offset_up/_dn() are accepted
 * until swmm_engine_initialize(). The check is the same as for the legacy
 * engine: slope (swmm_link_get_slope), peak flow in C1 and volume flooded at
 * J1 must equal those of a run with the edit in the input file, to a relative
 * 1e-6 plus 0.01 ft3 for the flooded volume. (6.0.0 has no full-flow getter.)
 *
 * Units per the headers: invert and offsets in project length units (ft),
 * flows in project flow units (cfs), elapsed time from swmm_engine_step in
 * days; slope is dimensionless.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

enum { NONE, NODE_ELEV, OFFSET_UP, OFFSET_DN };
typedef struct { double slope, qpeak, vflood; int rc, err; } Result;

static Result run(const char *inp, int what, double value)
{
    Result r = {0.0, 0.0, 0.0, 0, 0};
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, tPrev = 0.0, q = 0.0;
    int c1, j1;

    r.err = swmm_engine_open(e, inp, "API-10_6.rpt", "API-10_6.out", NULL);
    c1 = swmm_link_index(e, "C1");
    j1 = swmm_node_index(e, "J1");
    if (!r.err && what == NODE_ELEV) r.rc = swmm_node_set_invert_elev(e, j1, value);
    if (!r.err && what == OFFSET_UP) r.rc = swmm_link_set_offset_up(e, c1, value);
    if (!r.err && what == OFFSET_DN) r.rc = swmm_link_set_offset_dn(e, c1, value);
    if (!r.err) swmm_link_get_slope(e, c1, &r.slope);
    if (!r.err) r.err = swmm_engine_initialize(e);
    if (!r.err) r.err = swmm_engine_start(e, 0);
    while (!r.err)
    {
        r.err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_link_get_flow(e, c1, &q);
        if (q > r.qpeak) r.qpeak = q;
        swmm_node_get_overflow(e, j1, &q);
        r.vflood += q * (t - tPrev) * 86400.0;     /* cfs * s = ft3 */
        tPrev = t;
    }
    if (!r.err) r.err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return r;
}

static int differ(double a, double b, double absTol)
{
    return fabs(a - b) > 1.0e-6 * fmax(fabs(a), fabs(b)) + absTol;
}

int main(void)
{
    struct { const char *name; int what; double value; const char *inp; } cases[] = {
        { "invert J1 = 101",      NODE_ELEV, 101.0, "API-10_j1-101.inp"      },
        { "offset_up C1 = 1",     OFFSET_UP, 1.0,   "API-10_offset1-1.inp"   },
        { "offset_dn C1 = 0.5",   OFFSET_DN, 0.5,   "API-10_offset2-0.5.inp" },
    };
    Result b, a, x, first = {0};
    int i, nbad = 0;

    b = run("API-10_base.inp", NONE, 0.0);
    printf("API-10_base.inp unchanged: C1 slope %.6f, peak %.3f cfs, J1 flooded %.0f ft3\n\n",
           b.slope, b.qpeak, b.vflood);
    printf("%-22s %3s | %-17s | %-15s | %s\n", "Set before initialize", "rc",
           "C1 slope", "C1 peak flow", "J1 flooded (ft3)");
    printf("%-22s %3s | %8s %8s | %7s %7s | %8s %8s\n", "", "",
           "API", ".inp", "API", ".inp", "API", ".inp");
    for (i = 0; i < 3; i++)
    {
        a = run("API-10_base.inp", cases[i].what, cases[i].value);
        x = run(cases[i].inp, NONE, 0.0);
        if (i == 0) { first = a; b = x; }
        printf("%-22s %3d | %8.6f %8.6f | %7.3f %7.3f | %8.0f %8.0f\n",
               cases[i].name, a.rc, a.slope, x.slope, a.qpeak, x.qpeak, a.vflood, x.vflood);
        if (a.rc || a.err || x.err || differ(a.slope, x.slope, 0.0) ||
            differ(a.qpeak, x.qpeak, 0.0) || differ(a.vflood, x.vflood, 0.01)) nbad++;
    }
    if (nbad)
    {
        printf("FAIL: %d of 3 invert/offset setters returned 0 but C1 kept the slope computed "
               "at open (after raising J1: slope %.6f instead of %.6f, J1 flooded %.0f ft3 "
               "instead of %.0f)\n", nbad, first.slope, b.slope, first.vflood, b.vflood);
        return 1;
    }
    printf("PASS: invert and offsets set before initialize give the same slope and flows "
           "as the same values in the input file\n");
    return 0;
}
