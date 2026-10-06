/*
 * NUM-20 for 6.0.0: standard arch size code 31 has the full hydraulic radius
 * of code 30.
 *
 * Same check as NUM-20_test.c. Codes 30, 31 and 32 (corrugated steel, 3 x 1 in)
 * are the same pipe-arch shape at three scales (span/rise 1.29, 1.28, 1.29;
 * A/(rise*span) 0.813, 0.817, 0.815), so Rfull/rise must be the same for all
 * three. 6.0.0 exposes the section each link was built with, so the test
 * reads Yfull, Afull and Rfull directly (swmm_link_create_xsect +
 * swmm_xsect_full_properties). Tolerance 3 %: the neighbours agree to 0.1 %,
 * the defect is 14 % low.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_xsect.h"

int main(void)
{
    const char *ids[3] = {"A30", "A31", "A32"};
    double yfull[3], afull[3], rfull[3], wmax[3], ratio[3], ref, dev;
    int i, rc;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-20_arch-codes.inp", "NUM-20_6.rpt", "NUM-20_6.out", NULL);
    for (i = 0; !rc && i < 3; i++)
    {
        SWMM_XSect xs = NULL;
        rc = swmm_link_create_xsect(e, swmm_link_index(e, ids[i]), &xs);
        if (!rc) rc = swmm_xsect_full_properties(xs, &yfull[i], &afull[i], &rfull[i],
                                                 &wmax[i], NULL, NULL);
        swmm_xsect_free(xs);
        ratio[i] = rfull[i] / yfull[i];
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the engine returned error %d\n", rc); return 1; }

    printf("Code  Rise x span (in)  Afull (ft2)  A/(rise*span)  Rfull (ft)  Rfull/rise\n");
    for (i = 0; i < 3; i++)
        printf("%4d  %6.0f x %-6.0f   %8.1f      %8.3f   %9.3f   %9.3f\n",
               30 + i, 12.0 * yfull[i], 12.0 * wmax[i], afull[i],
               afull[i] / (yfull[i] * wmax[i]), rfull[i], ratio[i]);

    ref = 0.5 * (ratio[0] + ratio[2]);
    dev = ratio[1] / ref - 1.0;
    printf("Code 31: Rfull/rise %.3f vs %.3f for codes 30 and 32 (%+.1f %%); "
           "Rfull %.3f ft, %.3f ft by the same ratio\n", ratio[1], ref, 100.0 * dev,
           rfull[1], ref * yfull[1]);
    if (fabs(dev) > 0.03)
    {
        printf("FAIL: code 31 has Rfull = %.3f ft, %.0f %% below the %.3f ft its shape implies\n",
               rfull[1], -100.0 * dev, ref * yfull[1]);
        return 1;
    }
    printf("PASS: codes 30-32 have the same Rfull/rise, as similar shapes must\n");
    return 0;
}
