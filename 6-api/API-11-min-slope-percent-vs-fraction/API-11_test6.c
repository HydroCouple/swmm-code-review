/*
 * API-11 for 6.0.0: the MIN_SLOPE option read and written through
 * swmm_options_get/_set, and its effect when set after swmm_engine_open().
 *
 * Same checks as the legacy test: MIN_SLOPE 0.5 (percent) in [OPTIONS] reads
 * back as 0.5; 0.5 set through the API reads back as 0.5; and 0.5 set before
 * swmm_engine_initialize() gives C1 the slope (0.005) and J1 the flooding of
 * the deck with MIN_SLOPE 0.5, to a relative 1e-6 (plus 0.01 ft3 for the
 * flooded volume, which is 0 in one run). swmm_options_set is documented as
 * valid before swmm_engine_start().
 *
 * Units: options are strings in [OPTIONS] units; swmm_link_get_slope is
 * dimensionless; overflow in project flow units (cfs); elapsed time in days.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_model.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

typedef struct { double before, after, slope, vflood; int rc, err; } Result;

static double getMinSlope(SWMM_Engine e)
{
    char buf[64] = "";
    if (swmm_options_get(e, "MIN_SLOPE", buf, (int)sizeof buf) != 0) return -1.0;
    return atof(buf);
}

static Result run(const char *inp, const char *setValue)
{
    Result r = {0.0, 0.0, 0.0, 0.0, 0, 0};
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, tPrev = 0.0, q = 0.0;
    int c1, j1;

    r.err = swmm_engine_open(e, inp, "API-11_6.rpt", "API-11_6.out", NULL);
    c1 = swmm_link_index(e, "C1");
    j1 = swmm_node_index(e, "J1");
    r.before = getMinSlope(e);
    if (!r.err && setValue) r.rc = swmm_options_set(e, "MIN_SLOPE", setValue);
    r.after = getMinSlope(e);
    if (!r.err) r.err = swmm_engine_initialize(e);
    if (!r.err) swmm_link_get_slope(e, c1, &r.slope);
    if (!r.err) r.err = swmm_engine_start(e, 0);
    while (!r.err)
    {
        r.err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
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
    Result inp = run("API-11_minslope.inp", NULL);
    Result api = run("API-11_base.inp", "0.5");
    int bad = 0;

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
        printf("  effect: set before initialize, C1 slope %.6f and J1 flooding %.0f ft3; "
               "from the input file %.6f and %.0f ft3\n", api.slope, api.vflood, inp.slope, inp.vflood);
        bad++;
    }
    if (bad)
    {
        printf("FAIL: MIN_SLOPE is wrong in %d of 3 checks (set 0.5 leaves C1 at slope %.6f "
               "instead of %.6f)\n", bad, api.slope, inp.slope);
        return 1;
    }
    printf("PASS: MIN_SLOPE reads and writes percent like [OPTIONS], and a value set "
           "before initialize changes the conduit slopes as the input file does\n");
    return 0;
}
