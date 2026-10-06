/*
 * NUM-48: first-order decay in conduits and storage units uses the explicit
 * Euler factor (1 - K1 dt) instead of exp(-K1 dt), so results depend on the
 * routing step; and STEADY routing ignores a negative K1 (growth).
 *
 * The reference manual (Vol. III, eq. 5-6) defines the reactor update as
 * c(t+dt) = [c(t) V(t) exp(-K1 dt) + Cin Qin dt] / (V(t) + Qin dt), and
 * says SWMM 5 uses it for conduits and storage nodes. getReactedQual()
 * computes c * (1 - K1 dt) instead. Since 1 - x < exp(-x), every step
 * removes too much, by an amount that grows with K1 dt.
 * findSFLinkQual() (STEADY) does use exp(-K1 dt), but only if K1 > 0,
 * although the [POLLUTANTS] reader accepts a negative K1 for growth.
 *
 * Decks:
 *   pond-decay-10s/60s/300s  closed 4000 ft3 pond, 100 mg/L, K1 = 24/day,
 *                            KINWAVE, 6 h, routing step 10, 60 and 300 s.
 *                            Exact: 100 exp(-6) = 0.247875 mg/L for any step.
 *   steady-growth            STEADY, 1 cfs at 10 mg/L through conduit C1,
 *                            K1 = -24/day, 300 s step. Eq. 5-6 over the step:
 *                            10 exp(24/86400 * 300) = 10.869 mg/L in C1.
 *
 * Correct behaviour: the value of eq. 5-6. With exp() the per-step factors
 * multiply to exp(-K1 t) exactly, so the test allows 0.1 % (Euler is 0.8 %,
 * 4.9 % and 23 % low in the pond; skipping growth gives 10.000 in C1).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

struct deck { const char *inp; int isLink; const char *obj; int var; };

int main(void)
{
    const struct deck decks[] = {
        /* node results: depth, head, volume, lat. inflow, inflow, flooding, P1 */
        {"NUM-48_pond-decay-10s.inp",  0, "SU1", 6},
        {"NUM-48_pond-decay-60s.inp",  0, "SU1", 6},
        {"NUM-48_pond-decay-300s.inp", 0, "SU1", 6},
        /* link results: flow, depth, velocity, volume, capacity, P1 */
        {"NUM-48_steady-growth.inp",   1, "C1",  5}};
    int i, nbad = 0;

    printf("Deck                          Object  P1 at end   Eq. 5-6    Difference\n");
    printf("                                      (mg/L)      (mg/L)     (%%)\n");
    for (i = 0; i < 4; i++)
    {
        double t = 0.0, c = NAN, expected, diff;
        int rc, idx = -1, nper = 0, len = 0, bad;
        float *v = NULL;
        SMO_Handle h = NULL;

        /* closed pond: 100 exp(-K1 t), K1 = 24/day, t = 6 h;
           STEADY conduit: 10 exp(-K1 dt), K1 = -24/day, dt = 300 s */
        expected = (i < 3) ? 100.0 * exp(-24.0 / 86400.0 * 6.0 * 3600.0)
                           : 10.0 * exp(24.0 / 86400.0 * 300.0);

        rc = swmm_open(decks[i].inp, "NUM-48.rpt", "NUM-48.out");
        if (!rc) rc = swmm_start(1);
        if (!rc) idx = swmm_getIndex(decks[i].isLink ? swmm_LINK : swmm_NODE, decks[i].obj);
        while (!rc)
        {
            rc = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_report();
        swmm_close();

        SMO_init(&h);
        if (!rc && idx >= 0 && SMO_open(h, "NUM-48.out") == 0 &&
            SMO_getTimes(h, SMO_numPeriods, &nper) == 0 && nper > 0)
        {
            int err = decks[i].isLink ? SMO_getLinkResult(h, nper - 1, idx, &v, &len)
                                      : SMO_getNodeResult(h, nper - 1, idx, &v, &len);
            if (err == 0 && len > decks[i].var) c = v[decks[i].var];
        }
        SMO_free((void **)&v);
        SMO_close(&h);

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
