/*
 * CON-17 for 6.0.0 (same decks and checks as CON-17_test.c). XSection.cpp
 * setParams() uses the same formulas as legacy xsect_setParams().
 *
 * CON-17: a custom-sized elliptical pipe gets the full area and hydraulic
 * radius of a standard 1:1.56 ellipse, whatever its span.
 *
 * For HORIZ_ELLIPSE with no size code, xsect_setParams() sets
 *     aFull = 1.2692 * yFull^2,  rFull = 0.3061 * yFull      (span not used)
 * and for VERT_ELLIPSE the same with wMax in place of yFull (rise not used),
 * while the top width, and so the node surface area, is scaled by the span.
 *
 * The decks run a 15 cfs hydrograph through three flat 2000 ft conduits:
 *   CON-17_horiz-2x6.inp   HORIZ_ELLIPSE 2 ft high, 6 ft wide
 *   CON-17_vert-6x2.inp    VERT_ELLIPSE  6 ft high, 2 ft wide
 *
 * Correct behaviour:
 *  - the full area is close to that of an ellipse with these axes,
 *    pi/4 * 2 * 6 = 9.42 ft2. SWMM's standard elliptical shape is about 3 %
 *    fuller than a true ellipse (Table A12: 5.10 ft2 for 24 x 38 in, where
 *    pi/4 * 2 * 3.17 = 4.97 ft2), so 5 % is allowed; the wrong area is 46 % low.
 *  - the flow routing continuity error is small: the conduit volume (from the
 *    area table) and the node surface area (from the width table) then
 *    describe the same pipe. 1 % is allowed; the same model with a standard
 *    proportion pipe gives about -0.2 %, the inconsistent tables +3 %.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

/* full area, hydraulic radius and width of C1 from the Cross Section Summary */
static int readC1(const char *rpt, double *a, double *r, double *w)
{
    FILE *f = fopen(rpt, "r");
    char line[512], name[64], shape[64];
    double d;
    int in = 0, found = 0;
    if (!f) return 0;
    while (!found && fgets(line, sizeof line, f))
    {
        if (strstr(line, "Cross Section Summary")) { in = 1; continue; }
        if (in && sscanf(line, "%63s %63s %lf %lf %lf %lf", name, shape, &d, a, r, w) == 6
            && strcmp(name, "C1") == 0) found = 1;
    }
    fclose(f);
    return found;
}

static int run(const char *inp, const char *rpt, const char *out, double *ce)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, frac = 0.0;
    int rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_routing_continuity_error(e, &frac);
    *ce = 100.0 * frac;                              /* fraction -> percent */
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    static const char *deck[2] = {"CON-17_horiz-2x6", "CON-17_vert-6x2"};
    const double aEllipse = 0.25 * 3.14159265358979 * 2.0 * 6.0;   /* 9.42 ft2 */
    int i, nbad = 0;
    char bad[256] = "";

    printf("Deck                Full area  True ellipse  Ratio   HydRad  Width  Continuity\n");
    printf("                    (ft2)      (ft2)                 (ft)    (ft)   error (%%)\n");
    for (i = 0; i < 2; i++)
    {
        char inp[64], rpt[64], out[64];
        double a = 0, r = 0, w = 0, ce = 0;
        int err;
        snprintf(inp, sizeof inp, "%s.inp", deck[i]);
        snprintf(rpt, sizeof rpt, "%s.rpt", deck[i]);
        snprintf(out, sizeof out, "%s.out", deck[i]);
        err = run(inp, rpt, out, &ce);
        if (err || !readC1(rpt, &a, &r, &w))
        {
            printf("%-18s  run failed with error %d\n", deck[i], err);
            snprintf(bad + strlen(bad), sizeof bad - strlen(bad), "%s%s did not run",
                     nbad++ ? "; " : "", deck[i]);
            continue;
        }
        printf("%-18s  %9.2f  %12.2f  %5.3f  %6.2f  %5.2f  %10.3f\n", deck[i], a, aEllipse,
               a / aEllipse, r, w, ce);
        if (fabs(a / aEllipse - 1.0) > 0.05 || fabs(ce) > 1.0)
            snprintf(bad + strlen(bad), sizeof bad - strlen(bad),
                     "%s%s: full area %.2f ft2 (%.0f%% of the ellipse), continuity error %.3f%%",
                     nbad++ ? "; " : "", deck[i], a, 100.0 * a / aEllipse, ce);
    }
    if (nbad)
    {
        printf("FAIL: %s\n", bad);
        return 1;
    }
    printf("PASS: custom ellipses get the area of their own axes and conserve volume\n");
    return 0;
}
