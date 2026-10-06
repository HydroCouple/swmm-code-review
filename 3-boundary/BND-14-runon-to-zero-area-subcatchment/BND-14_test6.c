/*
 * BND-14 for 6.0.0: runoff sent to a subcatchment with zero area disappears.
 *
 * In the deck, S1 (1 acre) drains onto S2, whose Area is 0, and S2 drains to
 * outfall O1. SWMMEngine::assembleRunon() drops run-on to a subcatchment with
 * no area, and S1's runoff is not counted as leaving the study area because
 * its outlet is a subcatchment. The water is lost with no message.
 *
 * Correct behaviour is either to reject the input, or to deliver the water.
 * The test passes if the project fails to open, initialize or start with an
 * error, or if it runs with a runoff continuity error below 1 %. With the bug
 * it runs with a +95 % continuity error (0.9 in of rain, no runoff).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    double t = 0.0, runoffErr = 0.0;
    SWMM_Engine e = swmm_engine_create();
    int err = swmm_engine_open(e, "BND-14_runon-to-zero-area.inp",
                               "BND-14_6.rpt", "BND-14_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (err)
    {
        int i, n = swmm_get_error_count(e);
        printf("The project was rejected with error %d\n", err);
        for (i = 0; i < n; i++) printf("  %s\n", swmm_get_error_at(e, i));
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        printf("PASS: a subcatchment draining onto a zero-area subcatchment is "
               "reported as an input error\n");
        return 0;
    }
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) swmm_get_runoff_continuity_error(e, &runoffErr);   /* a fraction */
    runoffErr *= 100.0;
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    printf("The project ran with no error\n");
    printf("Runoff continuity error (%%)  %8.3f\n", runoffErr);
    if (fabs(runoffErr) > 1.0)
    {
        printf("FAIL: S1's runoff onto zero-area S2 is lost without a message: "
               "runoff continuity error %.3f %%\n", runoffErr);
        return 1;
    }
    printf("PASS: S1's runoff is delivered; runoff continuity error %.3f %%\n", runoffErr);
    return 0;
}
