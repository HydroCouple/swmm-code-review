/*
 * NUM-50 for 6.0.0: API mass flux under STEADY routing.
 *
 * 6.0.0 has no link pollutant flux, so the legacy defect has no counterpart.
 * The nearest API, swmm_node_set_quality_mass_flux(), is checked the same way
 * on the same deck: a flux of 10 set on J1 (the upstream node of C1) after the
 * first routing step must leave through the outfall, i.e. the quality
 * continuity error stays small. The bound is the legacy test's 5%
 * (steady-flow quality routing leaves 1-2% in transit at the end of a run).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, cLink = 0.0, cOut = 0.0, qerr = 0.0;
    int j1, c1, o1, step = 0;
    int rc = swmm_engine_open(e, "NUM-50_steady-conduit.inp", "NUM-50_6.rpt", "NUM-50_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    o1 = swmm_node_index(e, "O1");
    c1 = swmm_link_index(e, "C1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        if (++step == 1) rc = swmm_node_set_quality_mass_flux(e, j1, 0, 10.0);
        swmm_link_get_quality(e, c1, 0, &cLink);
        swmm_node_get_quality(e, o1, 0, &cOut);
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_quality_continuity_error(e, 0, &qerr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    printf("6.0.0 has no link flux API; node flux of 10 set on J1 instead\n");
    printf("Final step: C1 conc %.5f, O1 conc %.5f (mg/L), error code %d, "
           "quality continuity error %.3f %%\n", cLink, cOut, rc, qerr * 100.0);
    if (rc || fabs(qerr) > 0.05)
    {
        printf("FAIL: an API mass flux under STEADY routing is not conserved "
               "(continuity error %.3f %%)\n", qerr * 100.0);
        return 1;
    }
    printf("PASS: an API mass flux under STEADY routing leaves through the outfall "
           "(continuity error %.3f %%)\n", qerr * 100.0);
    return 0;
}
