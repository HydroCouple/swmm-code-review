/*
 * API-11: swmm_MINSLOPE in the 5.3.0 toolkit reads a fraction, is written
 * without conversion, and a value set before swmm_start() has no effect.
 *
 * [OPTIONS] MIN_SLOPE is a percent (input manual; project.c divides it by
 * 100). The minimum is applied to conduit slopes in conduit_getSlope(), which
 * runs during swmm_open(). Correct behaviour, checked here:
 *   1. the getter returns the option in the unit of the input file, so
 *      MIN_SLOPE 0.5 reads back as 0.5;
 *   2. a value set through the API reads back unchanged;
 *   3. MIN_SLOPE 0.5 set before swmm_start() gives the same model as
 *      MIN_SLOPE 0.5 in the input file: C1 (geometric slope 0.0025) gets
 *      slope 0.005, and the flooding at J1 is the same.
 * Values that must be equal must agree to a relative 1e-6 (plus 0.01 ft3 for
 * the flooded volume, which is 0 in one run). The defect gives 0.005 instead
 * of 0.5 in (1), slope 0.0025 instead of 0.005 in (3), and 862 ft3 of
 * flooding instead of none.
 *
 * 5.2.4 has no swmm_MINSLOPE property, so it is not affected.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
typedef struct { double before, after, slope, vflood; int rc, err; } Result;

/* Runs inp; if setValue >= 0, sets MINSLOPE to it before swmm_start(). */
static Result run(const char *inp, double setValue)
{
    Result r = {0.0, 0.0, 0.0, 0.0, 0, 0};
    double t = 0.0, tPrev = 0.0, q;
    int c1, j1;

    r.err = swmm_open(inp, "API-11.rpt", "API-11.out");
    if (r.err) { swmm_close(); return r; }
    c1 = swmm_getIndex(swmm_LINK, "C1");
    j1 = swmm_getIndex(swmm_NODE, "J1");
    r.before = swmm_getValueExpanded(swmm_SYSTEM, swmm_MINSLOPE, 0, 0, 0);
    if (setValue >= 0.0)
        r.rc = swmm_setValueExpanded(swmm_SYSTEM, swmm_MINSLOPE, 0, 0, 0, setValue);
    r.after = swmm_getValueExpanded(swmm_SYSTEM, swmm_MINSLOPE, 0, 0, 0);
    r.err = swmm_start(0);
    r.slope = swmm_getValueExpanded(swmm_LINK, swmm_LINK_SLOPE, c1, -1, -1);
    while (!r.err)
    {
        r.err = swmm_step(&t);
        if (t <= 0.0) break;
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
    Result inp, api;
    int bad = 0;

    inp = run("API-11_minslope.inp", -1.0);
    api = run("API-11_base.inp", 0.5);

    printf("                                     read      C1 slope   J1 flooded (ft3)\n");
    printf("MIN_SLOPE 0.5 in [OPTIONS]        %8g    %9.6f   %8.0f\n", inp.before, inp.slope, inp.vflood);
    printf("no MIN_SLOPE, API sets 0.5 (rc %d)  %8g    %9.6f   %8.0f\n", api.rc, api.after, api.slope, api.vflood);
    printf("  (read before the set: %g)\n", api.before);

    if (inp.err || api.err || api.rc) { printf("FAIL: error (open/run %d %d, set %d)\n", inp.err, api.err, api.rc); return 1; }
    if (differ(inp.before, 0.5, 0.0))
    {
        printf("  getter: MIN_SLOPE 0.5 from the input file reads %g, not 0.5\n", inp.before);
        bad++;
    }
    if (differ(api.after, 0.5, 0.0))
    {
        printf("  setter: 0.5 set through the API reads back %g\n", api.after);
        bad++;
    }
    if (differ(api.slope, inp.slope, 0.0) || differ(api.vflood, inp.vflood, 0.01))
    {
        printf("  effect: set before swmm_start, C1 slope %.6f and J1 flooding %.0f ft3; "
               "from the input file %.6f and %.0f ft3\n", api.slope, api.vflood, inp.slope, inp.vflood);
        bad++;
    }
    if (bad)
    {
        printf("FAIL: swmm_MINSLOPE is wrong in %d of 3 checks (read %g for MIN_SLOPE 0.5; "
               "set 0.5 leaves C1 at slope %.6f instead of %.6f)\n", bad, inp.before, api.slope, inp.slope);
        return 1;
    }
    printf("PASS: swmm_MINSLOPE reads and writes percent like [OPTIONS], and a value set "
           "before swmm_start() changes the conduit slopes as the input file does\n");
    return 0;
#else
    printf("PASS: not affected, 5.2.4 has no swmm_MINSLOPE property\n");
    return 0;
#endif
}
