/*
 * CON-24: the snow pack's cold content is capped 12 times below the
 * Reference Manual's limit.
 *
 * updateColdContent() (snow.c) limits the cold content to
 *     ccMax = wsnow * 0.007 / 12.0 * (tbase - ati)
 * with wsnow and the cold content both in ft of water equivalent. The
 * Reference Manual (Vol. I, Sec. 6.6, step 8) gives
 *     COLDC <= 0.007 WSNOW (Tbase - ATI),
 * both depths in the same unit, so the /12 makes the cap 12 times too small.
 *
 * CON-24_cold-spell.inp: a 4 in pack on a 1 ac pervious subcatchment, 72 h at
 * 2 degF, then 6 h at 50 degF (Tbase 32 degF, DHM 0.01 in/hr/degF, ATI weight
 * 0.5, RNM 1, no rain or losses, FWF 0). Expected from the manual:
 *   ATI after 72 h = 2 + 30 x 0.5^(72/6)                 = 2.0073 degF
 *   cold content   = 0.007 x 4 x (32 - 2.0073)           = 0.8398 in
 *     (the cap binds from the first cold step: each step's gain
 *      RNM x DHM x (ATI - Ta) x dt is about 6 times the cap's growth)
 *   melt potential = 0.01 x (50 - 32) x 6 h              = 1.0800 in
 *   melt           = 1.0800 - 0.8398 (RNM = 1, so the cold
 *                    content is repaid 1:1 by melt)       = 0.2402 in
 *   pack left      = 4 - 0.2402                          = 3.760 in
 * The cap with /12 gives 0.0700 in of cold content and 2.990 in left.
 * The test reads the pack left from the report's "Final Snow Cover" line and
 * accepts 3.760 +- 0.05 in (the faulty cap is 0.77 in away).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
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
    const double w0 = 4.0, tbase = 32.0, tcold = 2.0, twarm = 50.0;
    const double dhm = 0.01, warm_hours = 6.0;
    double ati, cc_manual, cc_code, potential, left_manual, left_code;
    double left, cc_seen, elapsed = 0.0;
    int err;

    ati = tcold + (tbase - tcold) * pow(0.5, 72.0 / 6.0);
    cc_manual = 0.007 * w0 * (tbase - ati);
    cc_code = cc_manual / 12.0;
    potential = dhm * (twarm - tbase) * warm_hours;
    left_manual = w0 - (potential - cc_manual);
    left_code = w0 - (potential - cc_code);

    err = swmm_open("CON-24_cold-spell.inp", "CON-24.rpt", "CON-24.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    left = final_snow_cover("CON-24.rpt");
    if (left < 0.0)
    {
        printf("FAIL: no Final Snow Cover line in the report\n");
        return 1;
    }
    /* cold content implied by the melt that did not happen (RNM = 1) */
    cc_seen = potential - (w0 - left);

    printf("4 in pack: 72 h at 2 degF (ATI -> %.4f degF), then 6 h at 50 degF\n",
           ati);
    printf("                                    Cold content (in)  Pack left (in)\n");
    printf("Manual 0.007 x WSNOW x (Tbase-ATI)      %7.4f           %7.3f\n",
           cc_manual, left_manual);
    printf("Same cap divided by 12                  %7.4f           %7.3f\n",
           cc_code, left_code);
    printf("This run                                %7.4f           %7.3f\n",
           cc_seen, left);

    /* the report prints 3 decimals; 0.05 in is 15 times smaller than the
       0.77 in between the results of the two caps */
    if (fabs(left - left_manual) > 0.05)
    {
        printf("FAIL: %.3f in of snow left instead of %.3f in: the cold content "
               "was capped at %.4f in, not 0.007 x WSNOW x (Tbase - ATI) = "
               "%.4f in\n", left, left_manual, cc_seen, cc_manual);
        return 1;
    }
    printf("PASS: the cold content reaches 0.007 x WSNOW x (Tbase - ATI) = "
           "%.4f in and %.3f in of snow is left\n", cc_manual, left);
    return 0;
}
