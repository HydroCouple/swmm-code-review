/*
 * NUM-04: SI snow melt coefficients are multiplied by 1.8 instead of divided.
 *
 * setMeltParams() (snow.c) converts the [SNOWPACKS] Cmin/Cmax coefficients to
 * the internal ft/sec per degF with x * UCF(TEMPERATURE) / UCF(RAINFALL). For
 * SI, UCF(TEMPERATURE) = 1.8, but a coefficient per degC is 1.8 times LARGER
 * than the same coefficient per degF, so it must be divided by 1.8. SI packs
 * therefore melt 1.8^2 = 3.24 times faster than specified.
 *
 * Both decks hold a 25.4 mm (1 in) pack under a constant air temperature
 * 10 degC (18 degF) above the base temperature, with no rain, for 1 hour. The
 * degree-day equation gives
 *     melt = C x (Ta - Tbase) x 1 h = 0.4572 mm/hr/degC x 10 degC = 4.572 mm
 *          = 0.01 in/hr/degF x 18 degF = 0.180 in.
 * The test reads the pack left at the end from the report's Runoff Quantity
 * Continuity table ("Final Snow Cover", depth column) and checks the melt
 * in each unit system against 4.572 mm within 5% (the faulty SI conversion
 * gives 3.24 x 4.572 = 14.81 mm).
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* depth column of "Final Snow Cover" in the report, or -1 */
static double final_snow_cover(const char *rpt)
{
    char line[256];
    double vol, depth = -1.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof line, f))
    {
        char *p = strstr(line, "Final Snow Cover");
        if (!p) continue;
        p += strlen("Final Snow Cover");
        while (*p == ' ' || *p == '.') p++;
        if (sscanf(p, "%lf %lf", &vol, &depth) != 2) depth = -1.0;
        break;
    }
    fclose(f);
    return depth;
}

int main(void)
{
    const double expected_mm = 4.572;
    double si_left, us_left, si_melt, us_melt;
    int err1, err2, ok;

    err1 = swmm_run("NUM-04_si.inp", "NUM-04_si.rpt", "NUM-04_si.out");
    err2 = swmm_run("NUM-04_us.inp", "NUM-04_us.rpt", "NUM-04_us.out");
    if (err1 || err2)
    {
        printf("FAIL: a run stopped with error %d / %d\n", err1, err2);
        return 1;
    }
    si_left = final_snow_cover("NUM-04_si.rpt");
    us_left = final_snow_cover("NUM-04_us.rpt");
    if (si_left < 0.0 || us_left < 0.0)
    {
        printf("FAIL: no Final Snow Cover line in a report\n");
        return 1;
    }
    si_melt = 25.4 - si_left;             /* mm */
    us_melt = (1.0 - us_left) * 25.4;     /* in -> mm */

    printf("\nMelt of a 25.4 mm pack in 1 h at 10 degC (18 degF) above base\n");
    printf("Units  Pack left        Melt (mm)  Expected (mm)  Ratio\n");
    printf("SI     %7.3f mm      %8.3f   %8.3f      %6.3f\n",
           si_left, si_melt, expected_mm, si_melt / expected_mm);
    printf("US     %7.3f in      %8.3f   %8.3f      %6.3f\n",
           us_left, us_melt, expected_mm, us_melt / expected_mm);

    ok = si_melt > 0.95 * expected_mm && si_melt < 1.05 * expected_mm
      && us_melt > 0.95 * expected_mm && us_melt < 1.05 * expected_mm;
    if (!ok)
    {
        printf("FAIL: the pack melted %.3f mm in SI units and %.3f mm in US "
               "units; the degree-day equation gives %.3f mm in both\n",
               si_melt, us_melt, expected_mm);
        return 1;
    }
    printf("PASS: both unit systems melt %.3f mm, as the degree-day equation "
           "gives\n", expected_mm);
    return 0;
}
