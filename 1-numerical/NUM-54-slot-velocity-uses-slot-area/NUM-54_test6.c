/*
 * NUM-54 for 6.0.0: 6.0.0 already uses the conveyance area (slot removed) in
 * the momentum equation (DynamicWave.cpp, getConveyArea), so this test is
 * expected to pass unpatched.
 *
 * Same decks and check as NUM-54_test.c: a horizontal 3 ft pipe, n = 0.015,
 * 1000 ft, full between a 20 ft fixed head and a 10 ft fixed stage, must carry
 * the Manning full-pipe flow 1.486/n*A*R^(2/3)*sqrt((h1-h2)/L) within 1%, with
 * SURCHARGE_METHOD SLOT and EXTRAN alike.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *q, double *h1, double *h2)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0;
    int rc, t1, o1, c1;

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    t1 = swmm_node_index(e, "T1");
    o1 = swmm_node_index(e, "O1");
    c1 = swmm_link_index(e, "C1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_link_get_flow(e, c1, q);    /* cfs (CFS model) */
        swmm_node_get_head(e, t1, h1);   /* ft */
        swmm_node_get_head(e, o1, h2);   /* ft */
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    const double D = 3.0, n = 0.015, L = 1000.0, PI = 3.14159265358979;
    const double A = PI * D * D / 4.0, R = D / 4.0;
    double qS = 0, h1S = 0, h2S = 0, qE = 0, h1E = 0, h2E = 0, qaS, qaE, eS, eE;
    int err = 0;

    err |= runDeck("NUM-54_slot.inp", "NUM-54_slot6.rpt", "NUM-54_slot6.out", &qS, &h1S, &h2S);
    err |= runDeck("NUM-54_extran.inp", "NUM-54_extran6.rpt", "NUM-54_extran6.out", &qE, &h1E, &h2E);
    if (err)
    {
        printf("FAIL: a run stopped with an error\n");
        return 1;
    }
    qaS = 1.486 / n * A * pow(R, 2.0 / 3.0) * sqrt((h1S - h2S) / L);
    qaE = 1.486 / n * A * pow(R, 2.0 / 3.0) * sqrt((h1E - h2E) / L);
    eS = qS / qaS - 1.0;
    eE = qE / qaE - 1.0;

    printf("Surcharge   head T1   head O1   Manning full-pipe Q   SWMM Q    error\n");
    printf("method        (ft)      (ft)         (cfs)            (cfs)\n");
    printf("SLOT        %6.3f    %6.3f      %9.3f          %7.3f   %+6.2f%%\n", h1S, h2S, qaS, qS, 100.0 * eS);
    printf("EXTRAN      %6.3f    %6.3f      %9.3f          %7.3f   %+6.2f%%\n", h1E, h2E, qaE, qE, 100.0 * eE);

    /* 1%: the slot-area velocity gives +5.1% here, the conveyance area ~0% */
    if (fabs(eS) > 0.01 || fabs(eE) > 0.01)
    {
        printf("FAIL: a full pipe under a fixed head difference carries %.3f cfs with "
               "SURCHARGE_METHOD SLOT, %+.2f%% off the Manning full-pipe flow %.3f cfs "
               "(EXTRAN %+.2f%%)\n", qS, 100.0 * eS, qaS, 100.0 * eE);
        return 1;
    }
    printf("PASS: the surcharged pipe carries the Manning full-pipe flow with both "
           "SLOT and EXTRAN\n");
    return 0;
}
