/*
 * BND-01 (legacy toolkit API, 5.2.4 and 5.3.0): a FREE or NORMAL outfall fed by a weir or orifice is given a depth
 * equal to the regulator's crest height above its UPSTREAM node.
 *
 * Two decks, each a storage unit (invert 100 ft) draining through a regulator
 * to an outfall whose invert lies above the storage invert but below the
 * regulator crest:
 *   BND-01_weir-free.inp      20 ft transverse weir, crest 105, FREE outfall
 *                             at 103, 20 cfs inflow
 *   BND-01_orifice-normal.inp 1 x 2 ft side orifice, bottom 103 / crown 104,
 *                             NORMAL outfall at 102, 5 cfs inflow
 *
 * Correct behaviour, from first principles:
 *   - A FREE or NORMAL outfall takes the critical or normal depth of its
 *     conduit; a regulator has neither (the engine uses 0), so the outfall
 *     depth is 0. Tolerance 0.001 ft; the bug gives 5 ft and 3 ft.
 *   - Such an outfall has no water of its own, so the regulator never flows
 *     back out of it: minimum link flow >= -0.001 cfs (the bug: -346 cfs).
 *   - Weir: at steady state Q = Cw L h^1.5 with Cw = 3.33, L = 20 ft,
 *     Q = 20 cfs, so h = (20 / 66.6)^(2/3) = 0.448 ft and the storage HGL
 *     is 105.448 ft. Tolerance 0.05 ft (the bug raises it by about 2.6 ft).
 *   - Orifice: discharging freely when just full (head 0.5 ft over its
 *     centre) it passes Cd A sqrt(2 g 0.5) = 0.65 * 2 * 5.67 = 7.4 cfs > 5
 *     cfs, so at steady state the storage stage lies below the orifice crown
 *     (104 ft). The bug's 105-ft tailwater forces it above 105.
 */
#include <stdio.h>
#include "swmm5.h"

typedef struct {
    const char *inp, *rpt, *out, *link;
    double hglMax;      /* storage HGL at the end must be <= this */
    double hglMin;      /* ... and >= this */
    double maxDepth, minFlow, hgl;  /* results */
} Case;

static int runCase(Case *c, int *ok)
{
    double t = 0.0, d, q, maxDepth = 0.0, minFlow = 1e10, maxFlow = -1e10, hgl = 0.0;
    int err, su, of, lk;

    err = swmm_open(c->inp, c->rpt, c->out);
    if (!err) err = swmm_start(1);
    if (err) { printf("%s: error %d\n", c->inp, err); swmm_close(); return err; }
    su = swmm_getIndex(swmm_NODE, "SU1");
    of = swmm_getIndex(swmm_NODE, "O1");
    lk = swmm_getIndex(swmm_LINK, c->link);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        d = swmm_getValue(swmm_NODE_DEPTH, of);
        q = swmm_getValue(swmm_LINK_FLOW, lk);
        if (d > maxDepth) maxDepth = d;
        if (q < minFlow) minFlow = q;
        if (q > maxFlow) maxFlow = q;
        hgl = swmm_getValue(swmm_NODE_HEAD, su);
    }
    swmm_end();
    swmm_close();

    printf("%-26s %10.3f %10.3f %10.3f %10.3f   %.3f-%.3f\n", c->inp, maxDepth,
           minFlow, maxFlow, hgl, c->hglMin, c->hglMax);
    if (maxDepth > 0.001 || minFlow < -0.001 || hgl > c->hglMax || hgl < c->hglMin) *ok = 0;
    c->maxDepth = maxDepth; c->minFlow = minFlow; c->hgl = hgl;
    return err;
}

int main(void)
{
    Case cases[2] = {
        {"BND-01_weir-free.inp", "BND-01_weir.rpt", "BND-01_weir.out", "W1",
         105.448 + 0.05, 105.448 - 0.05, 0, 0, 0},
        {"BND-01_orifice-normal.inp", "BND-01_orifice.rpt", "BND-01_orifice.out", "OR1",
         104.0, 103.0, 0, 0, 0}
    };
    int i, ok = 1, err = 0;

    printf("%-26s %10s %10s %10s %10s   %s\n", "Deck", "O1 depth", "min flow",
           "max flow", "SU1 HGL", "expected SU1 HGL");
    printf("%-26s %10s %10s %10s %10s\n", "", "max (ft)", "(cfs)", "(cfs)", "end (ft)");
    for (i = 0; i < 2; i++) err |= runCase(&cases[i], &ok);
    if (err)
    {
        printf("FAIL: a run stopped with an error\n");
        return 1;
    }
    if (!ok)
    {
        printf("FAIL: outfall depth %.3f ft (weir, FREE) and %.3f ft (orifice, NORMAL) "
               "instead of 0; minimum flow %.2f / %.2f cfs; storage HGL %.3f ft (expected "
               "105.448) / %.3f ft (expected < 104)\n", cases[0].maxDepth, cases[1].maxDepth,
               cases[0].minFlow, cases[1].minFlow, cases[0].hgl, cases[1].hgl);
        return 1;
    }
    printf("PASS: outfall depth stays 0, no reverse flow, storage stage matches free discharge\n");
    return 0;
}
