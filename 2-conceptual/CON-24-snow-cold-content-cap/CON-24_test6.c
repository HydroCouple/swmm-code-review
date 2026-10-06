/*
 * CON-24 for 6.0.0: the snow pack's cold content is capped 12 times below the
 * Reference Manual's limit (SnowSolver::updateColdContent(), Snow.cpp, as
 * legacy snow.c: ccMax = wsnow * 0.007 / 12.0 * (tbase - ati)).
 *
 * Same deck and check as CON-24_test.c: a 4 in pack, 72 h at 2 degF, then
 * 6 h at 50 degF. The manual's cap 0.007 x WSNOW x (Tbase - ATI) gives
 * 0.8398 in of cold content and 3.760 in of snow left; the /12 cap gives
 * 0.0700 in and 2.990 in. The pack left is read with
 * swmm_get_runoff_total(SWMM_RUNOFF_FINALSNOW) (ft3) over the 1 ac area.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    const double w0 = 4.0, tbase = 32.0, tcold = 2.0, twarm = 50.0;
    const double dhm = 0.01, warm_hours = 6.0, area_ft2 = 43560.0;
    double ati, cc_manual, cc_code, potential, left_manual, left_code;
    double vol = 0.0, left, cc_seen, t = 0.0;
    int rc;
    SWMM_Engine e;

    ati = tcold + (tbase - tcold) * pow(0.5, 72.0 / 6.0);
    cc_manual = 0.007 * w0 * (tbase - ati);
    cc_code = cc_manual / 12.0;
    potential = dhm * (twarm - tbase) * warm_hours;
    left_manual = w0 - (potential - cc_manual);
    left_code = w0 - (potential - cc_code);

    e = swmm_engine_create();
    rc = swmm_engine_open(e, "CON-24_cold-spell.inp", "CON-24_6.rpt",
                          "CON-24_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_get_runoff_total(e, SWMM_RUNOFF_FINALSNOW, &vol);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }
    left = vol / area_ft2 * 12.0;               /* ft3 -> in over 1 ac */
    cc_seen = potential - (w0 - left);          /* RNM = 1 */

    printf("4 in pack: 72 h at 2 degF (ATI -> %.4f degF), then 6 h at 50 degF\n",
           ati);
    printf("                                    Cold content (in)  Pack left (in)\n");
    printf("Manual 0.007 x WSNOW x (Tbase-ATI)      %7.4f           %7.3f\n",
           cc_manual, left_manual);
    printf("Same cap divided by 12                  %7.4f           %7.3f\n",
           cc_code, left_code);
    printf("This run                                %7.4f           %7.3f\n",
           cc_seen, left);

    /* 0.05 in is 15 times smaller than the 0.77 in between the results of
       the two caps */
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
