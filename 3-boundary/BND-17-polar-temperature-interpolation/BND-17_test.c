/*
 * BND-17: hourly temperatures interpolated from daily Tmax/Tmin are wrong in
 * polar winter.
 *
 * With climate-file temperatures, SWMM puts Tmin at sunrise and Tmax 3 h
 * before sunset and joins them with sine curves (Vol. I, Section 2.4). When
 * the day is shorter than 3 h, as at 70 N around the winter solstice, the
 * "3 h before sunset" hour falls before sunrise. The rising curve is then
 * never used: the temperature drops to Tmin at sunrise and jumps up the next
 * moment, and Tmax is never reached.
 *
 * Correct behaviour, whatever the day length: every day the interpolated
 * temperature reaches the day's Tmax and Tmin, and it changes smoothly. The
 * climate file has Tmax 40 F and Tmin 20 F every day. The test reads the air
 * temperature the run writes to the binary output every 10 minutes for Dec
 * 20 and 21 and checks, for each day:
 *   - the highest value is >= 39.5 F and the lowest <= 20.5 F;
 *   - no change between two consecutive 10-minute values exceeds 10 F,
 *     half the daily range.
 * The latitude 40 deck is a control: there the largest 10-minute change is
 * about 1 F. At 70 N the buggy engine jumps about 16 F at sunrise and never
 * goes above about 36.6 F; with the fix the rise from Tmin to Tmax takes 1 h,
 * which is about 5 F per 10 minutes at its steepest.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

/* runs one deck and checks it; returns 1 if the temperatures are right */
static int check(const char *inp, const char *rpt, const char *out)
{
    double elapsed = 0.0;
    int err, ok = 1, nper = 0, k, len = 0, day;
    SMO_Handle h = NULL;
    float *vals = NULL;
    double tmax[2] = {-1e9, -1e9}, tmin[2] = {1e9, 1e9}, jump[2] = {0, 0};
    double hmax[2] = {0, 0}, hjump[2] = {0, 0}, prev = -1e9;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("%s: run stopped with error %d\n", inp, err);
        return 0;
    }

    SMO_init(&h);
    if (SMO_open(h, out) != 0 || SMO_getTimes(h, SMO_numPeriods, &nper) != 0 || nper != 288)
    {
        printf("%s: cannot read 288 periods from %s\n", inp, out);
        return 0;
    }
    /* reports every 10 min starting at 00:10, so period k is at hour (k+1)/6;
       index 0 of the system results is the air temperature (deg F) */
    for (k = 0; k < nper; k++)
    {
        double hour = (k + 1) / 6.0, t;
        if (SMO_getSystemResult(h, k, 0, &vals, &len) != 0 || len < 1) { ok = 0; break; }
        t = vals[0];
        SMO_free((void **)&vals);
        day = (k + 1 < 144) ? 0 : 1;     /* hour 24:00 belongs to day 1 */
        if (hour >= 24.0) hour -= 24.0;
        if (t > tmax[day]) { tmax[day] = t; hmax[day] = hour; }
        if (t < tmin[day]) tmin[day] = t;
        if (prev > -1e9 && fabs(t - prev) > jump[day]) { jump[day] = fabs(t - prev); hjump[day] = hour; }
        prev = t;
    }
    SMO_close(&h);

    printf("%s\n  Day      Highest (at)       Lowest   Largest 10-min change (at)\n", inp);
    for (day = 0; day < 2; day++)
    {
        printf("  Dec %d  %6.2f F (%5.2f h)  %6.2f F  %6.2f F (%5.2f h)\n", 20 + day,
               tmax[day], hmax[day], tmin[day], jump[day], hjump[day]);
        if (tmax[day] < 39.5 || tmin[day] > 20.5 || jump[day] > 10.0) ok = 0;
    }
    return ok;
}

int main(void)
{
    int ok40 = check("BND-17_lat40-december.inp", "BND-17_lat40.rpt", "BND-17_lat40.out");
    int ok70 = check("BND-17_lat70-december.inp", "BND-17_lat70.rpt", "BND-17_lat70.out");

    if (!ok40 || !ok70)
    {
        printf("FAIL: the interpolated temperature at latitude %s does not reach Tmax 40 F "
               "or Tmin 20 F each day, or changes by more than 10 F in 10 minutes\n",
               !ok70 ? (!ok40 ? "40 and 70" : "70") : "40");
        return 1;
    }
    printf("PASS: at latitudes 40 and 70 the interpolated temperature reaches Tmax and "
           "Tmin each day and changes smoothly\n");
    return 0;
}
