/*
 * IO-26: in an SI model, wind speed read from a user-prepared climate file is
 * not converted from km/hr.
 *
 * The engine manual (Chapter 3, Climate Files) says a user-prepared climate
 * file holds wind speed in km/hour for metric units, the same unit as the
 * WINDSPEED MONTHLY values. SWMM uses wind speed only in the rain-on-snow
 * melt equation (snow.c getRainmelt), where the melt rate is a linear
 * function of wind speed.
 *
 * Three runs of the same SI deck (100 mm of snow, 2 h of 12.7 mm/hr rain at
 * 10 C) differ only in wind: from the climate file (32.18 km/hr), MONTHLY
 * 32.18 km/hr, and none. The wind-driven melt is melt(wind) - melt(none), so
 * the wind speed the engine took from the file is
 *     32.18 km/hr x (melt_file - melt_none) / (melt_monthly - melt_none).
 * Correct behaviour: 32.18 km/hr, i.e. the file and MONTHLY runs give the same
 * snow cover. The buggy engine treats 32.18 as mph and uses 51.75 km/hr
 * (ratio 1.608). Tolerance: the ratio must be within 2% of 1.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* value in mm (second column) of a line of the runoff continuity table */
static double rptValue(const char *rpt, const char *label)
{
    char line[256];
    double v1, v2 = -999.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -999.0;
    while (fgets(line, sizeof(line), f))
    {
        char *p = strstr(line, label);
        if (p)
        {
            p += strlen(label);                    /* skip the dot leader */
            while (*p == ' ' || *p == '.') p++;
            if (sscanf(p, "%lf %lf", &v1, &v2) == 2) break;
        }
    }
    fclose(f);
    return v2;
}

/* runs a deck and returns the snow melted (mm), or -1 on error */
static double melt(const char *inp, const char *rpt, const char *out)
{
    double elapsed = 0.0, init, final;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("%s: run stopped with error %d\n", inp, err);
        return -1.0;
    }
    init = rptValue(rpt, "Initial Snow Cover");
    final = rptValue(rpt, "Final Snow Cover");
    printf("  %-24s initial %8.3f mm  final %8.3f mm  melted %7.3f mm\n", inp, init, final, init - final);
    return init - final;
}

int main(void)
{
    double mFile, mMonthly, mNone, ratio;

    printf("SI deck, 100 mm of snow, 2 h of 12.7 mm/hr rain at 10 C\n");
    mFile = melt("IO-26_wind-file.inp", "IO-26_file.rpt", "IO-26_file.out");
    mMonthly = melt("IO-26_wind-monthly.inp", "IO-26_monthly.rpt", "IO-26_monthly.out");
    mNone = melt("IO-26_no-wind.inp", "IO-26_none.rpt", "IO-26_none.out");
    if (mFile < 0.0 || mMonthly < 0.0 || mNone < 0.0)
    {
        printf("FAIL: a run stopped with an error\n");
        return 1;
    }

    ratio = (mFile - mNone) / (mMonthly - mNone);
    printf("Wind-driven melt: climate file %.3f mm, MONTHLY %.3f mm, ratio %.3f (expected 1.000)\n",
           mFile - mNone, mMonthly - mNone, ratio);
    printf("Wind speed used from the file: %.2f km/hr (file value 32.18 km/hr)\n", 32.18 * ratio);

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
