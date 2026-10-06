/*
 * CRASH-12 for 6.0.0: steady-flow link quality with node index >= link count.
 *
 * 6.0.0 has no link pollutant flux API, and its steady-flow link quality step
 * (QualityRouting.cpp) only reads the conduit's own upstream node, so the
 * legacy defect has no counterpart. This runs scenario A of the legacy test:
 * the outfall is listed first, so J2 (upstream node of C2) has node index 2
 * with only 2 links. Correct: the run finishes, both conduits carry the
 * 10 mg/L that enters J1 (within 0.01 mg/L), and the quality continuity error
 * is under 5% (the legacy test's bound; steady-flow quality routing leaves
 * about 1-2% on this deck in every engine).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, conc1 = 0.0, conc2 = 0.0, qerr = 0.0;
    int c1, c2, ok = 1;
    int rc = swmm_engine_open(e, "CRASH-12_outfall-first.inp", "CRASH-12_A6.rpt",
                              "CRASH-12_A6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    c1 = swmm_link_index(e, "C1");
    c2 = swmm_link_index(e, "C2");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_link_get_quality(e, c1, 0, &conc1);
        swmm_link_get_quality(e, c2, 0, &conc2);
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_quality_continuity_error(e, 0, &qerr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    printf("Scenario B: not applicable (6.0.0 has no link pollutant flux API)\n");
    printf("Scenario A: error code %d, final concentration C1 = %.5f mg/L  C2 = %.5f mg/L, "
           "quality continuity error %.3f %%\n", rc, conc1, conc2, qerr * 100.0);
    if (rc || fabs(conc1 - 10.0) > 0.01 || fabs(conc2 - 10.0) > 0.01 || fabs(qerr) > 0.05)
        ok = 0;
    if (!ok)
    {
        printf("FAIL: steady-flow link quality is wrong when a node index exceeds the link count\n");
        return 1;
    }
    printf("PASS: steady-flow link quality uses each conduit's own upstream node\n");
    return 0;
}
