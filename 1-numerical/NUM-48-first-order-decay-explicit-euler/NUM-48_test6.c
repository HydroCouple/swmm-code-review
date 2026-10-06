/*
 * NUM-48 for 6.0.0: first-order decay in conduits and storage units uses the
 * explicit Euler factor (1 - K1 dt) instead of exp(-K1 dt), so results depend
 * on the routing step; and STEADY routing ignores a negative K1 (growth).
 *
 * 6.0.0 copies 5.3.0: QualitySolver::mixAtNodes() and the link kernel use
 * c1 * (1 - k * dt), and the STEADY branch applies exp(-k dt) only if k > 0.
 *
 * Decks and expected values as in NUM-48_test.c:
 *   pond-decay-10s/60s/300s  closed pond, 100 mg/L, K1 = 24/day, KINWAVE,
 *                            6 h: exact 100 exp(-6) = 0.247875 mg/L.
 *   steady-growth            STEADY, 10 mg/L through C1, K1 = -24/day,
 *                            300 s step: 10 exp(24/86400 * 300) = 10.869.
 * The test reads the concentration after the last routing step through
 * swmm_node_get_quality() / swmm_link_get_quality() and allows 0.1 %
 * (Euler is 0.8 %, 4.9 % and 23 % low; skipping growth gives 10.000).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

struct deck { const char *inp; int isLink; const char *obj; };

int main(void)
{
    const struct deck decks[] = {
        {"NUM-48_pond-decay-10s.inp",  0, "SU1"},
        {"NUM-48_pond-decay-60s.inp",  0, "SU1"},
        {"NUM-48_pond-decay-300s.inp", 0, "SU1"},
        {"NUM-48_steady-growth.inp",   1, "C1"}};
    int i, nbad = 0;

    printf("Deck                          Object  P1 at end   Eq. 5-6    Difference\n");
    printf("                                      (mg/L)      (mg/L)     (%%)\n");
    for (i = 0; i < 4; i++)
    {
        double t = 0.0, c = NAN, expected, diff;
        int rc, idx = -1, bad;

        /* closed pond: 100 exp(-K1 t), K1 = 24/day, t = 6 h;
           STEADY conduit: 10 exp(-K1 dt), K1 = -24/day, dt = 300 s */
        expected = (i < 3) ? 100.0 * exp(-24.0 / 86400.0 * 6.0 * 3600.0)
                           : 10.0 * exp(24.0 / 86400.0 * 300.0);

        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i].inp, "NUM-48_6.rpt", "NUM-48_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        if (!rc) idx = decks[i].isLink ? swmm_link_index(e, decks[i].obj)
                                       : swmm_node_index(e, decks[i].obj);
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (rc || t <= 0.0) break;
            if (decks[i].isLink) swmm_link_get_quality(e, idx, 0, &c);
            else                 swmm_node_get_quality(e, idx, 0, &c);
        }
        if (!rc) rc = swmm_engine_end(e);
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);

        diff = 100.0 * (c - expected) / expected;
        bad = rc || !(fabs(diff) < 0.1);
        if (bad) nbad++;
        printf("%-28s  %-6s  %9.6f   %9.6f  %8.2f%s\n", decks[i].inp, decks[i].obj, c, expected,
               diff, bad ? "  <-- wrong" : "");
        if (rc) printf("    run stopped with error %d\n", rc);
    }
    if (nbad)
    {
        printf("FAIL: in %d of 4 decks first-order reaction does not follow exp(-K1 dt): the pond "
               "result depends on the routing step and STEADY routing ignores growth\n", nbad);
        return 1;
    }
    printf("PASS: first-order decay and growth follow exp(-K1 dt) for every step size and routing method\n");
    return 0;
}
