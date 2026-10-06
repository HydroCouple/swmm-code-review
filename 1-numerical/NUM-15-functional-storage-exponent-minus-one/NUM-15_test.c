/*
 * NUM-15: a FUNCTIONAL storage unit with exponent A2 <= -1 is accepted.
 *
 * SWMM computes a FUNCTIONAL unit's volume as the integral of its area,
 * Area = A0 + A1 * Depth^A2, from depth 0:
 *     V(d) = A0 d + A1 / (A2 + 1) * d^(A2 + 1)
 * For A1 != 0 that integral only exists when A2 > -1. At A2 = -1 the code
 * divides by zero; below -1 the formula returns a finite value for an
 * integral that is infinite (and negative near the bottom).
 *
 * Correct behaviour: a unit whose volume cannot exist is rejected as an
 * input error (as a negative A0 already is). The decks: A1 = 50, A0 = 100,
 * initial depth 1 ft, 2 cfs inflow, with A2 = -1 and -2 (must be rejected)
 * and A2 = 0.5 (valid: must run, with finite volumes and continuity error).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

/* returns the error code of swmm_open/swmm_start (0 if the deck ran) */
static int run(const char *name, double *v0, double *vmax, float *contErr)
{
    char inp[64], rpt[64], out[64];
    double t = 0.0, v;
    float rErr = 0, qErr = 0;
    int k, err;

    sprintf(inp, "NUM-15_%s.inp", name);
    sprintf(rpt, "NUM-15_%s.rpt", name);
    sprintf(out, "NUM-15_%s.out", name);
    *v0 = *vmax = NAN;
    *contErr = NAN;
    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    if (!err)
    {
        k = swmm_getIndex(swmm_NODE, "S1");
        *v0 = *vmax = swmm_getValue(swmm_NODE_VOLUME, k);
        while (!err)
        {
            err = swmm_step(&t);
            v = swmm_getValue(swmm_NODE_VOLUME, k);
            if (!(v <= *vmax)) *vmax = v;
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_getMassBalErr(&rErr, contErr, &qErr);
    }
    swmm_report();
    swmm_close();
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
