/*
 * IO-20: an invalid number in [SUBAREAS] is reported as ERROR 209
 * "undefined object" instead of ERROR 211 "invalid number".
 *
 * Two decks, each with one bad value in the [SUBAREAS] line of S1:
 *   IO-20_nperv-abc.inp       N-Perv = abc   (not a number)
 *   IO-20_sperv-negative.inp  S-Perv = -0.5  (negative depth)
 *
 * Correct behaviour: the input is rejected with ERROR 211 (invalid number),
 * the code every other numeric check in subcatch.c uses. With the bug 5.2.4
 * and 5.3.0 reject both with "ERROR 209: undefined object abc" / "-0.5",
 * which sends the user looking for a missing object.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* prints the report's ERROR lines; returns 1 if one of them is ERROR 211 */
static int errorLines(const char* rpt)
{
    char line[512];
    int found = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        if (!strstr(line, "ERROR")) continue;
        printf("    %s", line + strspn(line, " "));
        if (strstr(line, "ERROR 211")) found = 1;
    }
    fclose(f);
    return found;
}

/* returns 0 if the deck is rejected with ERROR 211, 1 otherwise */
static int check(const char* inp, const char* rpt, const char* out)
{
    double elapsed = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    printf("%s:\n", inp);
    if (err)
    {
        swmm_close();
        printf("  rejected, error code %d; the report says:\n", err);
        if (errorLines(rpt)) return 0;
        printf("  -> not reported as ERROR 211 (invalid number)\n");
        return 1;
    }
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    printf("  accepted: the run finished, runoff continuity error %.3f %%\n", runoffErr);
    return 1;
}

int main(void)
{
    int bad = 0;
    bad += check("IO-20_nperv-abc.inp", "IO-20_abc.rpt", "IO-20_abc.out");
    bad += check("IO-20_sperv-negative.inp", "IO-20_neg.rpt", "IO-20_neg.out");
    if (bad)
    {
        printf("FAIL: %d of 2 invalid [SUBAREAS] numbers are not reported as "
               "ERROR 211 (invalid number)\n", bad);
        return 1;
    }
    printf("PASS: both invalid [SUBAREAS] numbers are reported as ERROR 211\n");
    return 0;
}
