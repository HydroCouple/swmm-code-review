/*
 * BND-14: runoff sent to a subcatchment with zero area disappears.
 *
 * In the deck, S1 (1 acre) drains onto S2, whose Area is 0 (accepted by the
 * [SUBCATCHMENTS] parser), and S2 drains to outfall O1. S2 has no surface to
 * receive S1's runoff and is skipped by the runoff calculation, while S1's
 * runoff is not counted as leaving the study area because its outlet is a
 * subcatchment. The water is lost with no message.
 *
 * Correct behaviour is either to reject the input, or to deliver the water.
 * The test passes if the project fails to open or start with an error, or if
 * it runs with a runoff continuity error below 1 %. With the bug it runs with
 * a +95 % continuity error (0.9 in of rain, no runoff).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double elapsed = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    char msg[512] = "";
    int err;

    err = swmm_open("BND-14_runon-to-zero-area.inp", "BND-14.rpt", "BND-14.out");
    if (!err) err = swmm_start(1);
    if (err)
    {
        swmm_getError(msg, sizeof(msg));
        swmm_close();
        printf("The project was rejected with error %d:%s\n", err, msg);
        printf("PASS: a subcatchment draining onto a zero-area subcatchment is "
               "reported as an input error\n");
        return 0;
    }
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
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
