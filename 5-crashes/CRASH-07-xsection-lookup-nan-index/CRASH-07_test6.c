/*
 * CRASH-07 for 6.0.0: the same three decks through the 6.0.0 API.
 * 6.0.0's lookup() is guarded and it rejects a "nan" diameter, but its
 * critical-depth code still converts a NaN estimate to int (getYcrit, from a
 * NaN initial flow), and its tabular_getdSdA() has the unguarded conversion.
 * The legacy description follows.
 *
 * lookup() in xsect.c finds the table segment of a normalised value x (a
 * depth or area ratio, nominally 0..1) with i = (int)(x / delta) and checks
 * only i >= nItems - 1. A NaN or a very large x makes the conversion
 * undefined behaviour (x86 returns INT_MIN), and the table is then read at
 * that negative index: a segmentation fault. tabular_getdSdA() has the same
 * code, and getYcritEnum() converts a NaN critical-depth estimate the same way.
 *
 * Three one-conduit decks reach these conversions:
 *   CRASH-07_tiny-diameter.inp  CIRCULAR 1e-30 ft: a valid number; the
 *                               initial depth ratio y/yFull is about 1e26
 *   CRASH-07_nan-diameter.inp   CIRCULAR nan (xsect_setParams rejects only
 *                               p[0] <= 0, which NaN passes)
 *   CRASH-07_nan-initflow.inp   CONDUITS InitFlow nan
 * Correct: no crash and no undefined behaviour. A run may end with an input
 * or run-time error; whether "nan" should be rejected as input is IO-01.
 * Under the sanitizers the unpatched engines stop with "runtime error: ... is
 * outside the range of representable values of type 'int'" and a SEGV.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"

static int runDeck(const char *inp, const char *rpt, const char *out)
{
    double elapsed = 0.0;
    int err;
    SWMM_Engine e = swmm_engine_create();
    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    const char *decks[3] = {"tiny-diameter", "nan-diameter", "nan-initflow"};
    char inp[64], rpt[64], out[64];
    int d, err;

    for (d = 0; d < 3; d++)
    {
        snprintf(inp, sizeof inp, "CRASH-07_%s.inp", decks[d]);
        snprintf(rpt, sizeof rpt, "CRASH-07_%s6.rpt", decks[d]);
        snprintf(out, sizeof out, "CRASH-07_%s6.out", decks[d]);
        printf("running %-16s ... ", decks[d]);
        fflush(stdout);
        err = runDeck(inp, rpt, out);
        printf("finished, error code %d\n", err);
    }
    printf("PASS: all three decks ran to the end without a crash\n");
    return 0;
}
