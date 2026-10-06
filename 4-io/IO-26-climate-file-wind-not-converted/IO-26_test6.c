/*
 * IO-26 for 6.0.0: in an SI model, wind speed read from a user-prepared
 * climate file is not converted from km/hr.
 *
 * Same decks and check as IO-26_test.c. Rain-on-snow melt is linear in wind
 * speed, so the wind speed the engine took from the file (32.18 km/hr) is
 *     32.18 x (melt_file - melt_none) / (melt_monthly - melt_none),
 * and must be 32.18 km/hr (ratio within 2% of 1). The buggy engine uses
 * 51.75 km/hr (ratio 1.608).
 *
 * The melt is (initial - final snow cover) / initial snow cover x 100 mm from
 * swmm_get_runoff_total(), so the volume units cancel. The test also prints
 * swmm_climate_get_wind_speed() (km/hr for an SI deck) for the climate-file
 * run.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"
#include "openswmm/engine/openswmm_forcing.h"

/* runs a deck and returns the snow melted (mm), or -1 on error */
static double melt(const char *inp, const char *rpt, const char *out, double *wind)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, init = 0.0, final = 0.0, m;
    int rc;

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_climate_get_wind_speed(e, wind);      /* km/hr */
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc)
    {
        swmm_get_runoff_total(e, SWMM_RUNOFF_INITSNOW, &init);
        swmm_get_runoff_total(e, SWMM_RUNOFF_FINALSNOW, &final);
    }
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc || init <= 0.0)
    {
        printf("%s: run stopped with error %d\n", inp, rc);
        return -1.0;
    }
    m = 100.0 * (init - final) / init;
    printf("  %-24s initial %8.3f mm  final %8.3f mm  melted %7.3f mm\n", inp, 100.0, 100.0 - m, m);
    return m;
}

int main(void)
{
    double mFile, mMonthly, mNone, ratio, wFile = 0.0, w = 0.0;

    printf("SI deck, 100 mm of snow, 2 h of 12.7 mm/hr rain at 10 C\n");
    mFile = melt("IO-26_wind-file.inp", "IO-26_file6.rpt", "IO-26_file6.out", &wFile);
    mMonthly = melt("IO-26_wind-monthly.inp", "IO-26_monthly6.rpt", "IO-26_monthly6.out", &w);
    mNone = melt("IO-26_no-wind.inp", "IO-26_none6.rpt", "IO-26_none6.out", &w);
    if (mFile < 0.0 || mMonthly < 0.0 || mNone < 0.0)
    {
        printf("FAIL: a run stopped with an error\n");
        return 1;
    }

    ratio = (mFile - mNone) / (mMonthly - mNone);
    printf("Wind-driven melt: climate file %.3f mm, MONTHLY %.3f mm, ratio %.3f (expected 1.000)\n",
           mFile - mNone, mMonthly - mNone, ratio);
    printf("Wind speed used from the file: %.2f km/hr (file value 32.18 km/hr)\n", 32.18 * ratio);
    printf("swmm_climate_get_wind_speed() in the climate-file run: %.2f km/hr\n", wFile);

    if (ratio < 0.98 || ratio > 1.02)
    {
        printf("FAIL: the climate file's wind speed of 32.18 km/hr is used as %.2f km/hr "
               "(%.3f x); final snow cover %.3f mm instead of %.3f mm\n",
               32.18 * ratio, ratio, 100.0 - mFile, 100.0 - mMonthly);
        return 1;
    }
    printf("PASS: the climate file's wind speed is read in km/hr, as documented for SI units\n");
    return 0;
}
