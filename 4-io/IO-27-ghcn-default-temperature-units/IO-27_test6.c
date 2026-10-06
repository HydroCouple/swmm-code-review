/*
 * IO-27 for 6.0.0: a GHCN-Daily climate file used without the Units token
 * must be read in tenths of a degree C, the default given by the input
 * reference ([TEMPERATURE] FILE: "C10 for tenths of a degree C (the
 * default)").
 *
 * Same decks and check as IO-27_test.c. TMAX 250 and TMIN 150 are 25 C and
 * 15 C, so the hourly air temperature must stay between 59 and 77 F in the
 * US-unit deck and between 15 and 25 C in the SI deck (within 0.5 degrees).
 * As in the legacy test, the temperature is read from the binary output
 * (system variable SWMM_OUT_SYS_TEMPERATURE, user units) for every hourly
 * report period.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_output.h"

static int check(const char *inp, const char *rpt, const char *out,
                 double lo, double hi, const char *unit)
{
    SWMM_Engine e = swmm_engine_create();
    SWMM_Output h;
    double t = 0.0, tmin = 1e9, tmax = -1e9;
    float v = 0.0f;
    int rc, k, nper;

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("%s: run stopped with error %d\n", inp, rc);
        return 0;
    }

    h = swmm_output_open(out);
    nper = h ? swmm_output_get_period_count(h) : 0;
    if (nper < 1)
    {
        printf("%s: cannot read %s\n", inp, out);
        if (h) swmm_output_close(h);
        return 0;
    }
    for (k = 0; k < nper; k++)
    {
        if (swmm_output_get_system_result(h, k, SWMM_OUT_SYS_TEMPERATURE, &v) != 0) break;
        if (v < tmin) tmin = v;
        if (v > tmax) tmax = v;
    }
    swmm_output_close(h);

    printf("  %-22s hourly air temperature %7.2f to %7.2f %s  (expected %.0f to %.0f %s)\n",
           inp, tmin, tmax, unit, lo, hi, unit);
    return k == nper && tmin >= lo - 0.5 && tmax <= hi + 0.5;
}

int main(void)
{
    int okUS, okSI;
    printf("GHCN file with TMAX 250, TMIN 150 (tenths of deg C), no Units token\n");
    okUS = check("IO-27_us-units.inp", "IO-27_us6.rpt", "IO-27_us6.out", 59.0, 77.0, "F");
    okSI = check("IO-27_si-units.inp", "IO-27_si6.rpt", "IO-27_si6.out", 15.0, 25.0, "C");

    if (!okUS || !okSI)
    {
        printf("FAIL: without a Units token the GHCN temperatures are not read in tenths of "
               "a degree C, the documented default (%s)\n",
               (!okUS && !okSI) ? "US and SI decks" : (!okUS ? "US deck" : "SI deck"));
        return 1;
    }
    printf("PASS: without a Units token the GHCN temperatures are read in tenths of a degree C\n");
    return 0;
}
