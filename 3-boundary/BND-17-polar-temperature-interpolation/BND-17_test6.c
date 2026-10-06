/*
 * BND-17 for 6.0.0: hourly temperatures interpolated from daily Tmax/Tmin are
 * wrong in polar winter.
 *
 * Same decks and checks as BND-17_test.c. The climate file has Tmax 40 F and
 * Tmin 20 F every day; for each of Dec 20 and 21 the air temperature must
 * reach >= 39.5 F and <= 20.5 F, and no change between two consecutive
 * 10-minute steps may exceed 10 F (half the daily range). The latitude 40
 * deck is a control (largest change about 1 F). At 70 N, 6.0.0 puts both
 * sunrise and the time of Tmax at noon when the sun does not rise, so the
 * temperature steps from Tmin to Tmax at once.
 *
 * swmm_climate_get_temperature() returns deg F for a US-unit deck; it is the
 * temperature computed for the runoff step just taken, read after every
 * 10-minute step. swmm_engine_step() returns elapsed days.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_forcing.h"

static int check(const char *inp, const char *rpt, const char *out)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, temp = 0.0, prev = -1e9;
    double tmax[2] = {-1e9, -1e9}, tmin[2] = {1e9, 1e9}, jump[2] = {0, 0};
    double hmax[2] = {0, 0}, hjump[2] = {0, 0};
    int rc, day, ok = 1;

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        double hour;
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_climate_get_temperature(e, &temp);     /* deg F */
        hour = t * 24.0;
        day = (hour < 24.0 - 1e-6) ? 0 : 1;         /* hour 24:00 belongs to day 1 */
        if (day == 1) hour -= 24.0;
        if (temp > tmax[day]) { tmax[day] = temp; hmax[day] = hour; }
        if (temp < tmin[day]) tmin[day] = temp;
        if (prev > -1e9 && fabs(temp - prev) > jump[day]) { jump[day] = fabs(temp - prev); hjump[day] = hour; }
        prev = temp;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("%s: run stopped with error %d\n", inp, rc);
        return 0;
    }

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
    int ok40 = check("BND-17_lat40-december.inp", "BND-17_lat40_6.rpt", "BND-17_lat40_6.out");
    int ok70 = check("BND-17_lat70-december.inp", "BND-17_lat70_6.rpt", "BND-17_lat70_6.out");

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
