/*
 * API-10: the 5.3.0 setters for a node's invert elevation and a link's
 * offsets do not update the conduit slope, full-flow capacity or the
 * kinematic-wave flow that depend on them.
 *
 * The slope of a conduit, beta = 1.49 sqrt(slope) / n and the full flow
 * qFull = beta * A * R^(2/3) are computed once, in conduit_validate() during
 * swmm_open(). swmm_setValueExpanded() accepts NODE_ELEV, LINK_OFFSET1 and
 * LINK_OFFSET2 between swmm_open() and swmm_start(), but only stores them.
 *
 * Correct behaviour: a value set through the API before swmm_start() gives
 * the same model as the same value written in the input file. For each
 * setter the test opens API-10_base.inp, sets the value, reads LINK_SLOPE and
 * LINK_FULLFLOW of C1, runs, and compares those values, the peak flow in C1
 * and the volume flooded at J1 with a run of the deck that has the edit in
 * [JUNCTIONS] or [CONDUITS]. Both runs use the same model and arithmetic, so
 * the numbers must agree to rounding: relative 1e-6, plus 0.01 ft3 for the
 * flooded volume, which is 0 in some runs. The defect changes the slope by a
 * factor of 2 and the flooded volume by thousands of ft3.
 *
 * 5.2.4 has no node invert or link offset setter, so it is not affected.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
typedef struct { double slope, qfull, qpeak, vflood; int rc, err; } Result;

/* Runs inp; if objType >= 0, sets prop of object id to value before swmm_start(). */
static Result run(const char *inp, int objType, int prop, const char *id, double value)
{
    Result r = {0.0, 0.0, 0.0, 0.0, 0, 0};
    double t = 0.0, tPrev = 0.0, q;
    int c1, j1;

    r.err = swmm_open(inp, "API-10.rpt", "API-10.out");
    if (r.err) { swmm_close(); return r; }
    c1 = swmm_getIndex(swmm_LINK, "C1");
    j1 = swmm_getIndex(swmm_NODE, "J1");
    if (objType >= 0)
        r.rc = swmm_setValueExpanded(objType, prop, swmm_getIndex(objType, id), -1, -1, value);
    r.slope = swmm_getValueExpanded(swmm_LINK, swmm_LINK_SLOPE, c1, -1, -1);
    r.qfull = swmm_getValueExpanded(swmm_LINK, swmm_LINK_FULLFLOW, c1, -1, -1);
    r.err = swmm_start(0);
    while (!r.err)
    {
        r.err = swmm_step(&t);
        if (t <= 0.0) break;
        q = swmm_getValueExpanded(swmm_LINK, swmm_LINK_FLOW, c1, -1, -1);
        if (q > r.qpeak) r.qpeak = q;
        q = swmm_getValueExpanded(swmm_NODE, swmm_NODE_OVERFLOW, j1, -1, -1);
        r.vflood += q * (t - tPrev) * 86400.0;     /* cfs * s = ft3 */
        tPrev = t;
    }
    swmm_end();
    swmm_close();
    return r;
}

static int differ(double a, double b, double absTol)
{
    return fabs(a - b) > 1.0e-6 * fmax(fabs(a), fabs(b)) + absTol;
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    struct { const char *name; int objType, prop; const char *id; double value; const char *inp; } cases[] = {
        { "NODE_ELEV J1 = 101",     swmm_NODE, swmm_NODE_ELEV,    "J1", 101.0, "API-10_j1-101.inp"      },
        { "LINK_OFFSET1 C1 = 1",    swmm_LINK, swmm_LINK_OFFSET1, "C1", 1.0,   "API-10_offset1-1.inp"   },
        { "LINK_OFFSET2 C1 = 0.5",  swmm_LINK, swmm_LINK_OFFSET2, "C1", 0.5,   "API-10_offset2-0.5.inp" },
    };
    Result b, a, e, first = {0};
    int i, nbad = 0;

    b = run("API-10_base.inp", -1, 0, NULL, 0.0);
    printf("API-10_base.inp unchanged: C1 slope %.6f, full flow %.3f cfs, peak %.3f cfs, "
           "J1 flooded %.0f ft3\n\n", b.slope, b.qfull, b.qpeak, b.vflood);
    printf("%-22s %3s | %-17s | %-15s | %-15s | %s\n", "Set before swmm_start", "rc",
           "C1 slope", "C1 full flow", "C1 peak flow", "J1 flooded (ft3)");
    printf("%-22s %3s | %8s %8s | %7s %7s | %7s %7s | %8s %8s\n", "", "",
           "API", ".inp", "API", ".inp", "API", ".inp", "API", ".inp");
    for (i = 0; i < 3; i++)
    {
        a = run("API-10_base.inp", cases[i].objType, cases[i].prop, cases[i].id, cases[i].value);
        e = run(cases[i].inp, -1, 0, NULL, 0.0);
        if (i == 0) { first = a; b = e; }
        printf("%-22s %3d | %8.6f %8.6f | %7.3f %7.3f | %7.3f %7.3f | %8.0f %8.0f\n",
               cases[i].name, a.rc, a.slope, e.slope, a.qfull, e.qfull,
               a.qpeak, e.qpeak, a.vflood, e.vflood);
        if (a.rc || a.err || e.err || differ(a.slope, e.slope, 0.0) ||
            differ(a.qfull, e.qfull, 0.0) || differ(a.qpeak, e.qpeak, 0.0) ||
            differ(a.vflood, e.vflood, 0.01)) nbad++;
    }
    if (nbad)
    {
        printf("FAIL: %d of 3 invert/offset setters returned 0 but C1 kept the slope and "
               "capacity computed at swmm_open (after raising J1: slope %.6f instead of %.6f, "
               "J1 flooded %.0f ft3 instead of %.0f)\n",
               nbad, first.slope, b.slope, first.vflood, b.vflood);
        return 1;
    }
    printf("PASS: NODE_ELEV, LINK_OFFSET1 and LINK_OFFSET2 set before swmm_start() give the "
           "same slope, capacity and flows as the same values in the input file\n");
    return 0;
#else
    printf("PASS: not affected, 5.2.4 has no node invert or link offset setter\n");
    return 0;
#endif
}
