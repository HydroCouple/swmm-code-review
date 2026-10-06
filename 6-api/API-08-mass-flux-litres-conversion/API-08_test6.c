/*
 * API-08 for 6.0.0: units of swmm_node_set_quality_mass_flux().
 *
 * The header documents the rate as "mass/sec (project mass units)", which the
 * manual defines for [INFLOWS] MASS as the pollutant's own mass unit (mg/s for
 * a MG/L pollutant). 1 cfs of clean flow passes J1 -> C1 -> O1. 10 mg/s of P1
 * must give 10 / 28.3168 = 0.3531 mg/L in C1 at steady state, whether it comes
 * from [INFLOWS] MASS (reference run) or from the API on J1 (set right after
 * swmm_engine_start). Checked within 3% after 2 h of constant flow.
 * (6.0.0 has no link mass flux.)
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

static int runCase(const char *inp, const char *rpt, const char *out, int useApi,
                   double *conc, int *setRc)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0;
    int j1, c1, rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    c1 = swmm_link_index(e, "C1");
    if (!rc && useApi) rc = *setRc = swmm_node_set_quality_mass_flux(e, j1, 0, 10.0);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_link_get_quality(e, c1, 0, conc);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    const double cExpected = 10.0 / 28.316846592;
    double cRef = 0, cNode = 0;
    int setRc = 0;
    int e1 = runCase("API-08_inflows-mass.inp", "API-08_6ref.rpt", "API-08_6ref.out", 0, &cRef, &setRc);
    int e2 = runCase("API-08_api-flux.inp", "API-08_6node.rpt", "API-08_6node.out", 1, &cNode, &setRc);

    printf("10 mg/s of P1 into 1 cfs; expected C1 concentration 10 / 28.3168 = %.4f mg/L\n", cExpected);
    printf("%-46s %16s\n", "source", "C1 conc (mg/L)");
    printf("%-46s %16.4f\n", "[INFLOWS] J1 P1 MASS baseline 10", cRef);
    printf("%-46s %16.4f\n", "swmm_node_set_quality_mass_flux(J1, P1, 10)", cNode);
    if (e1 || e2) { printf("FAIL: run error %d %d (set returned %d)\n", e1, e2, setRc); return 1; }
    if (fabs(cRef - cExpected) > 0.03 * cExpected || fabs(cNode - cExpected) > 0.03 * cExpected)
    {
        printf("FAIL: 10 mg/s gives %.4f mg/L through the API, expected %.4f mg/L "
               "([INFLOWS] MASS gives %.4f)\n", cNode, cExpected, cRef);
        return 1;
    }
    printf("PASS: an API mass flux of 10 mg/s gives %.4f mg/L in 1 cfs, as [INFLOWS] MASS does\n",
           cNode);
    return 0;
}
