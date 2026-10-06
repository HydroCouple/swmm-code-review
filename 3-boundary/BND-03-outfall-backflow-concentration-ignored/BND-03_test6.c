/*
 * BND-03 for 6.0.0 (C API): a CONCEN inflow at an outfall does not give the
 * reverse flow its concentration.
 *
 * The input reference ([INFLOWS]) says a CONCEN inflow needs a FLOW inflow
 * "unless the node is an Outfall. In that case a pollutant can enter the
 * system during periods when the outfall is submerged and reverse flow
 * occurs." In both decks the FIXED outfall OHI (stage 2 ft above its invert)
 * supplies about 13.7 cfs through J1 to the free outfall OLO, and has a
 * CONCEN inflow of 100 mg/L TSS:
 *   BND-03_concen-only.inp      the CONCEN inflow alone
 *   BND-03_concen-and-flow.inp  plus a 0.01 cfs FLOW inflow at OHI
 *
 * Correct behaviour: every drop reaching J1 comes from OHI, which carries
 * 100 mg/L, so after 12 h of steady flow J1 and OLO are at 100 mg/L.
 * Tolerance 1 mg/L; the bug gives 0 mg/L.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

/* runs a deck and returns TSS at nodes J1 and OLO after the last step */
static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *cJ1, double *cOLO)
{
    double t = 0.0;
    int rc, j1, olo;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    olo = swmm_node_index(e, "OLO");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_node_get_quality(e, j1, 0, cJ1);     /* mg/L */
        swmm_node_get_quality(e, olo, 0, cOLO);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    const char *decks[2] = {"BND-03_concen-only.inp", "BND-03_concen-and-flow.inp"};
    const char *rpts[2]  = {"BND-03_only6.rpt", "BND-03_flow6.rpt"};
    const char *outs[2]  = {"BND-03_only6.out", "BND-03_flow6.out"};
    double cJ1[2] = {0, 0}, cOLO[2] = {0, 0};
    int i, err, ok = 1;

    printf("%-28s %12s %12s %10s\n", "Deck", "J1 TSS", "OLO TSS", "expected");
    printf("%-28s %12s %12s %10s\n", "", "(mg/L)", "(mg/L)", "(mg/L)");
    for (i = 0; i < 2; i++)
    {
        err = runDeck(decks[i], rpts[i], outs[i], &cJ1[i], &cOLO[i]);
        if (err)
        {
            printf("FAIL: %s stopped with error %d\n", decks[i], err);
            return 1;
        }
        printf("%-28s %12.3f %12.3f %10.1f\n", decks[i], cJ1[i], cOLO[i], 100.0);
        if (fabs(cJ1[i] - 100.0) > 1.0 || fabs(cOLO[i] - 100.0) > 1.0) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: the reverse flow from the outfall does not carry its 100 mg/L CONCEN "
               "inflow: J1 is at %.3f mg/L (CONCEN only) and %.3f mg/L (CONCEN + FLOW)\n",
               cJ1[0], cJ1[1]);
        return 1;
    }
    printf("PASS: the reverse flow from the outfall carries the outfall's CONCEN inflow "
           "(100 mg/L)\n");
    return 0;
}
