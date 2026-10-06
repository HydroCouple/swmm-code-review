/*
 * API-08: units of 5.3.0's pollutant mass flux setters
 *   swmm_NODE_POLLUTANT_LATMASS_FLUX and swmm_LINK_POLLUTANT_LATMASS_FLUX.
 *
 * The API documents the flux as mass/sec in the pollutant's mass units (mg/s
 * for a MG/L pollutant), the same unit as an [INFLOWS] MASS inflow. 1 cfs of
 * clean flow passes J1 -> C1 -> O1. A source of 10 mg/s of P1 must give, at
 * steady state,
 *     c = 10 mg/s / (1 ft3/s x 28.3168 L/ft3) = 0.3531 mg/L
 * in C1, whether it is entered in [INFLOWS] as MASS (reference run) or set
 * through the API on J1 or on C1 right after swmm_start. Checked within 3%
 * after 2 h of constant flow. Without the litre conversion the API gives
 * 10 mg/L (28.3 times too much).
 * The value read back through the getter must be the value set (10).
 *
 * 5.2.4 has neither API and is not affected.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
/* objType < 0: no API call (reference deck) */
static int runCase(const char *inp, const char *rpt, const char *out,
                   int objType, double *conc, double *readBack)
{
    double elapsed = 0.0;
    int err, c1, idx = -1, prop = 0;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    c1 = swmm_getIndex(swmm_LINK, "C1");
    if (objType == swmm_NODE)
    {
        idx = swmm_getIndex(swmm_NODE, "J1");
        prop = swmm_NODE_POLLUTANT_LATMASS_FLUX;
    }
    else if (objType == swmm_LINK)
    {
        idx = c1;
        prop = swmm_LINK_POLLUTANT_LATMASS_FLUX;
    }
    if (!err && objType >= 0)
    {
        err = swmm_setValueExpanded(objType, prop, idx, 0, 0, 10.0);
        *readBack = swmm_getValueExpanded(objType, prop, idx, 0, 0);
    }
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        *conc = swmm_getValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_CONCENTRATION, c1, 0, 0);
    }
    swmm_end();
    swmm_report();
    swmm_close();
    return err;
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    const double cExpected = 10.0 / 28.316846592;
    double cRef = 0, cNode = 0, cLink = 0, backNode = 0, backLink = 0;
    int e1, e2, e3, bad = 0;

    e1 = runCase("API-08_inflows-mass.inp", "API-08_ref.rpt", "API-08_ref.out", -1, &cRef, &backNode);
    e2 = runCase("API-08_api-flux.inp", "API-08_node.rpt", "API-08_node.out", swmm_NODE, &cNode, &backNode);
    e3 = runCase("API-08_api-flux.inp", "API-08_link.rpt", "API-08_link.out", swmm_LINK, &cLink, &backLink);

    printf("10 mg/s of P1 into 1 cfs; expected C1 concentration 10 / 28.3168 = %.4f mg/L\n", cExpected);
    printf("%-44s %10s %16s\n", "source", "read back", "C1 conc (mg/L)");
    printf("%-44s %10s %16.4f\n", "[INFLOWS] J1 P1 MASS baseline 10", "-", cRef);
    printf("%-44s %10.4f %16.4f\n", "API NODE_POLLUTANT_LATMASS_FLUX(J1) = 10", backNode, cNode);
    printf("%-44s %10.4f %16.4f\n", "API LINK_POLLUTANT_LATMASS_FLUX(C1) = 10", backLink, cLink);
    if (e1 || e2 || e3) { printf("FAIL: run error %d %d %d\n", e1, e2, e3); return 1; }

    if (fabs(cRef - cExpected) > 0.03 * cExpected) bad++;
    if (fabs(cNode - cExpected) > 0.03 * cExpected || fabs(backNode - 10.0) > 1e-9) bad++;
    if (fabs(cLink - cExpected) > 0.03 * cExpected || fabs(backLink - 10.0) > 1e-9) bad++;
    if (bad)
    {
        printf("FAIL: 10 mg/s gives %.4f mg/L through the node API and %.4f mg/L through the "
               "link API, expected %.4f mg/L ([INFLOWS] MASS gives %.4f)\n",
               cNode, cLink, cExpected, cRef);
        return 1;
    }
    printf("PASS: an API mass flux of 10 mg/s gives %.4f mg/L in 1 cfs, as [INFLOWS] MASS "
           "does, and reads back as 10\n", cNode);
    return 0;
#else
    printf("5.2.4 has no pollutant mass flux API\n");
    printf("PASS: not affected (the API does not exist in 5.2.4)\n");
    return 0;
#endif
}
