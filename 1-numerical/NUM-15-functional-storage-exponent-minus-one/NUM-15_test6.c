/*
 * NUM-15 for 6.0.0: a FUNCTIONAL storage unit with exponent A2 <= -1 is accepted
 * (NodesHandler.cpp; volume in Node.cpp, as legacy node.c).
 *
 * Same decks and rule as NUM-15_test.c: the volume integral of
 * A0 + A1 * Depth^A2 from depth 0 does not exist for A1 != 0 and A2 <= -1,
 * so such a unit must be rejected; A2 = 0.5 must run with finite results.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

/* returns the error code of swmm_open/swmm_start (0 if the deck ran) */
static int run(const char *name, double *v0, double *vmax, float *contErr)
{
    char inp[64], rpt[64], out[64];
    double t = 0.0, v, ce = NAN;
    int k, err;
    SWMM_Engine e = swmm_engine_create();

    sprintf(inp, "NUM-15_%s.inp", name);
    sprintf(rpt, "NUM-15_%s6.rpt", name);
    sprintf(out, "NUM-15_%s6.out", name);
    *v0 = *vmax = NAN;
    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (!err)
    {
        k = swmm_node_index(e, "S1");
        swmm_node_get_volume(e, k, v0);
        *vmax = *v0;
        while (!err)
        {
            err = swmm_engine_step(e, &t);
            swmm_node_get_volume(e, k, &v);
            if (!(v <= *vmax)) *vmax = v;
            if (t <= 0) break;
        }
        if (!err) err = swmm_engine_end(e);
        swmm_get_routing_continuity_error(e, &ce);   /* a fraction */
        if (!err) err = swmm_engine_report(e);
    }
    *contErr = (float)(100.0 * ce);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    const char *names[3] = {"a2-minus1", "a2-minus2", "a2-plus0.5"};
    const int mustReject[3] = {1, 1, 0};
    double v0, vmax;
    float cont;
    int i, err, bad = 0;

    printf("Deck         Result                 V(1 ft)    Max volume  Continuity (%%)\n");
    for (i = 0; i < 3; i++)
    {
        err = run(names[i], &v0, &vmax, &cont);
        if (err)
            printf("%-11s  rejected (error %d)\n", names[i], err);
        else
            printf("%-11s  ran                  %9.3f  %11.3f  %14.3f\n",
                   names[i], v0, vmax, cont);
        if (mustReject[i] && !err) bad++;
        if (!mustReject[i] && (err || !isfinite(vmax) || !isfinite(cont)))
            bad++;
    }
    if (bad)
    {
        printf("FAIL: %d of 3 decks handled wrongly: a FUNCTIONAL unit with "
               "A2 <= -1 has no finite volume but is run\n", bad);
        return 1;
    }
    printf("PASS: exponents A2 <= -1 are rejected and a valid exponent runs\n");
    return 0;
}
