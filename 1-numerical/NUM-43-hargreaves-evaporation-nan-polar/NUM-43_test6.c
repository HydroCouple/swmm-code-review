/*
 * NUM-43 for 6.0.0: temperature-based (Hargreaves) evaporation above the
 * polar circles.
 *
 * Same decks and same check as NUM-43_test.c: at latitude 70 N, in early June
 * (sun never sets) and early December (sun never rises), the evaporation rate
 * must be the Hargreaves value with the sunset hour angle set to pi or 0, and
 * the runoff continuity error must be finite. 6.0.0 clamps the acos()
 * argument (src/engine/hydrology/Climate.cpp), so this test is expected to
 * pass unpatched.
 *
 * The rate is read with swmm_climate_get_evap_rate() (in/day for a US model)
 * at the first step at or after noon each day. Tolerance: 2% of the expected
 * rate plus 1e-4 in/day; a NaN fails it.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_forcing.h"
#include "openswmm/engine/openswmm_massbalance.h"

#define PI 3.141592654

static double hargreaves(int day, double lat, double tave, double trng)
{
    double a = 2.0 * PI / 365.0;
    double ta = (tave - 32.0) * 5.0 / 9.0;
    double tr = trng * 5.0 / 9.0;
    double lamda = 2.50 - 0.002361 * ta;
    double dr = 1.0 + 0.033 * cos(a * day);
    double phi = lat * 2.0 * PI / 360.0;
    double del = 0.4093 * sin(a * (284. + (double)day));
    double x = -tan(phi) * tan(del);
    double omega, ra;
    if (x < -1.0) x = -1.0;     /* sun never sets: omega = pi */
    if (x > 1.0) x = 1.0;       /* sun never rises: omega = 0 */
    omega = acos(x);
    ra = 37.6 * dr * (omega * sin(phi) * sin(del) + cos(phi) * cos(del) * sin(omega));
    return 0.0023 * ra / lamda * sqrt(tr) * (ta + 17.8) / 25.4;
}

static int check(const char *inp, const char *rpt, const char *out, int firstDay)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, rate = 0.0, runoffErr = 0.0;
    int rc, ok = 1, nextNoon = 0;

    printf("%s\n  Day   Noon PET (in/day)   Hargreaves (in/day)\n", inp);
    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        if (t >= nextNoon + 0.5)      /* elapsed time is in days */
        {
            int day = firstDay + nextNoon;
            double expect = hargreaves(day, 70.0, 50.0, 20.0);
            swmm_climate_get_evap_rate(e, &rate);
            printf("  %3d   %17.5f   %19.5f\n", day, rate, expect);
            if (!(fabs(rate - expect) <= 0.02 * expect + 1.0e-4)) ok = 0;
            nextNoon++;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) swmm_get_runoff_continuity_error(e, &runoffErr);   /* a fraction */
    runoffErr *= 100.0;
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("  run stopped with error %d\n", rc);
        return 0;
    }
    printf("  Runoff continuity error: %.3f %%\n", runoffErr);
    if (nextNoon < 4 || !isfinite(runoffErr) || fabs(runoffErr) > 1.0) ok = 0;
    return ok;
}

int main(void)
{
    int okJun = check("NUM-43_lat70-june.inp", "NUM-43_june6.rpt", "NUM-43_june6.out", 153);
    int okDec = check("NUM-43_lat70-december.inp", "NUM-43_december6.rpt", "NUM-43_december6.out", 336);

    if (!okJun || !okDec)
    {
        printf("FAIL: at latitude 70 N the temperature-based evaporation rate is not the "
               "Hargreaves value (%s%s%s), or the runoff balance is not finite\n",
               okJun ? "" : "June", (!okJun && !okDec) ? " and " : "", okDec ? "" : "December");
        return 1;
    }
    printf("PASS: at latitude 70 N the evaporation rate follows the Hargreaves equation "
           "in polar day and polar night and the runoff balance closes\n");
    return 0;
}
