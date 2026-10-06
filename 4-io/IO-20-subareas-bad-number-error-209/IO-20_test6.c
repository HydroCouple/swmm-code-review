/*
 * IO-20 for 6.0.0: invalid numbers in [SUBAREAS] are accepted silently.
 *
 * Two decks, each with one bad value in the [SUBAREAS] line of S1:
 *   IO-20_nperv-abc.inp       N-Perv = abc   (not a number)
 *   IO-20_sperv-negative.inp  S-Perv = -0.5  (negative depth)
 *
 * Correct behaviour: the input is rejected with ERROR 211 (invalid number),
 * as legacy SWMM rejects it (with the error code fixed by this issue). 6.0.0's
 * handle_subareas() converts every value with to_double() and no check, so
 * "abc" runs as n = 0 and -0.5 in as a negative depression storage.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

/* returns 0 if the deck is rejected with ERROR 211, 1 otherwise */
static int check(const char* inp, const char* rpt, const char* out)
{
    double t = 0.0, runoffErr = 0.0;
    SWMM_Engine e = swmm_engine_create();
    int err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    printf("%s:\n", inp);
    if (err)
    {
        int i, n = swmm_get_error_count(e), found = 0;
        printf("  rejected, error code %d; the engine says:\n", err);
        for (i = 0; i < n; i++)
        {
            const char* m = swmm_get_error_at(e, i);
            printf("    %s\n", m + strspn(m, " "));
            if (strstr(m, "ERROR 211")) found = 1;
        }
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        if (found) return 0;
        printf("  -> not reported as ERROR 211 (invalid number)\n");
        return 1;
    }
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) swmm_get_runoff_continuity_error(e, &runoffErr);   /* a fraction */
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    printf("  accepted: the run finished, runoff continuity error %.3f %%\n",
           100.0 * runoffErr);
    return 1;
}

int main(void)
{
    int bad = 0;
    bad += check("IO-20_nperv-abc.inp", "IO-20_abc_6.rpt", "IO-20_abc_6.out");
    bad += check("IO-20_sperv-negative.inp", "IO-20_neg_6.rpt", "IO-20_neg_6.out");
    if (bad)
    {
        printf("FAIL: %d of 2 invalid [SUBAREAS] numbers are not reported as "
               "ERROR 211 (invalid number)\n", bad);
        return 1;
    }
    printf("PASS: both invalid [SUBAREAS] numbers are reported as ERROR 211\n");
    return 0;
}
