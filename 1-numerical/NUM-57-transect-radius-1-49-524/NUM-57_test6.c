/*
 * NUM-57 for 6.0.0: the transect hydraulic-radius table must be consistent
 * with Manning's equation as used elsewhere (PHI = 1.486).
 *
 * Same deck as NUM-57_test.c: a rectangular transect 20 ft wide and 5 ft
 * deep, n = 0.03, no overbanks. Its full hydraulic radius must be
 * A/P = 100/30 = 3.333 ft. 6.0.0 exposes the section the link was built
 * with (swmm_link_create_xsect, swmm_xsect_full_properties), so the test
 * reads Afull and Rfull directly. Tolerance 0.05 % (5.2.4's 1.49 gives
 * -0.40 %).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_xsect.h"

int main(void)
{
    const double a = 100.0, p = 30.0;
    double afull = 0, rfull = 0;
    int rc;
    SWMM_XSect xs = NULL;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-57_rect-transect.inp", "NUM-57_6.rpt", "NUM-57_6.out", NULL);
    if (!rc) rc = swmm_link_create_xsect(e, swmm_link_index(e, "C1"), &xs);
    if (!rc) rc = swmm_xsect_full_properties(xs, NULL, &afull, &rfull, NULL, NULL, NULL);
    swmm_xsect_free(xs);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the engine returned error %d\n", rc); return 1; }

    printf("                          engine      exact\n");
    printf("Full area (ft2)        %10.4f %10.4f\n", afull, a);
    printf("Full hyd. radius (ft)  %10.4f %10.4f   (%+.3f %%)\n", rfull, a / p, 100.0 * (rfull / (a / p) - 1.0));

    if (fabs(afull / a - 1.0) > 5.0e-4 || fabs(rfull / (a / p) - 1.0) > 5.0e-4)
    {
        printf("FAIL: full hydraulic radius %.4f ft instead of A/P = %.4f ft\n", rfull, a / p);
        return 1;
    }
    printf("PASS: the transect's full hydraulic radius is A/P\n");
    return 0;
}
