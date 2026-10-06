/*
 * NUM-43: temperature-based (Hargreaves) evaporation is NaN above the polar
 * circles.
 *
 * getTempEvap() computes the sunset hour angle as acos(-tan(lat)*tan(decl)).
 * At latitude 70 N the argument is below -1 in early June (the sun never
 * sets) and above +1 in early December (the sun never rises), so acos()
 * returns NaN and so does the evaporation rate.
 *
 * Correct behaviour, from the Hargreaves equation used by SWMM (Vol. I,
 * Eq. 2-10 and 2-11) with the standard treatment of the polar cases (FAO-56): the
 * hour angle is pi when the sun never sets and 0 when it never rises. The
 * test computes that rate for each day, with the climate file's constant
 * Tmax 60 F / Tmin 40 F (so the 7-day moving averages are exactly 50 F and
 * 20 F), and compares it with the potential evaporation the engine writes to
 * the binary output at noon each day. It also checks that the runoff
 * continuity error is a finite number.
 *
 * Tolerance: 2% of the expected rate (plus 1e-4 in/day for the December
 * case, where the rate is 0). The buggy engine gives NaN, the fixed one the
 * formula to single precision.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

#define PI 3.141592654

/* Hargreaves rate (in/day) for day of year `day`, latitude `lat` (deg),
   7-day average temperature `tave` and range `trng` (deg F) */
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

/* runs one deck; returns 1 if the results are right */
static int check(const char *inp, const char *rpt, const char *out, int firstDay)
{
    double elapsed = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, ok = 1, nper = 0, k, len = 0;
    SMO_Handle h = NULL;
    float *vals = NULL;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    if (err)
    {
        printf("%s: run stopped with error %d\n", inp, err);
        return 0;
    }

    printf("%s\n  Day   Noon PET (in/day)   Hargreaves (in/day)\n", inp);
    SMO_init(&h);
    if (SMO_open(h, out) != 0 || SMO_getTimes(h, SMO_numPeriods, &nper) != 0)
    {
        printf("  cannot read %s\n", out);
        return 0;
    }
    /* hourly reports starting at 01:00, so period k is at hour k+1;
       index 14 of the system results is the potential evaporation rate */
    for (k = 11; k < nper; k += 24)
    {
        int day = firstDay + (k + 1) / 24;
        double expect = hargreaves(day, 70.0, 50.0, 20.0);
        double pet;
        if (SMO_getSystemResult(h, k, 0, &vals, &len) != 0 || len < 15) { ok = 0; break; }
        pet = vals[14];
        printf("  %3d   %17.5f   %19.5f\n", day, pet, expect);
        if (!(fabs(pet - expect) <= 0.02 * expect + 1.0e-4)) ok = 0;
        SMO_free((void **)&vals);
    }
    SMO_close(&h);
    printf("  Runoff continuity error: %.3f %%\n", runoffErr);
    if (!isfinite(runoffErr) || fabs(runoffErr) > 1.0) ok = 0;
    return ok;
}

int main(void)
{
    /* 1 June 2020 is day 153 and 1 December 2020 is day 336 (leap year) */
    int okJun = check("NUM-43_lat70-june.inp", "NUM-43_june.rpt", "NUM-43_june.out", 153);
    int okDec = check("NUM-43_lat70-december.inp", "NUM-43_december.rpt", "NUM-43_december.out", 336);

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
