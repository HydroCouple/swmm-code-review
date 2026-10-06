/*
 * IO-19: [COVERAGES] percentages are not checked.
 *
 * A [COVERAGES] percent is the "percent of the subcatchment's area covered
 * by the land use", so each value lies between 0 and 100 and the land uses
 * of one subcatchment cover at most 100 % together. The reader only checks
 * that the token is a number. Buildup and EMC washoff are scaled by the
 * stored fraction, so an excess adds pollutant load in proportion.
 *
 * The decks hold the same 10-ac subcatchment with two land uses, RES and COM,
 * both with an EMC of 100 mg/L of TSS and no buildup function:
 *   IO-19_split-100.inp    RES 60, COM 40     valid
 *   IO-19_single-160.inp   RES 160            invalid (a value above 100)
 *   IO-19_total-130.inp    RES 60, COM 70     invalid (130 % in total)
 *   IO-19_negative.inp     RES 100, COM -50   invalid (a negative value)
 * Correct: the valid deck runs, and the three invalid decks are rejected with
 * an input error when the project is opened.
 *
 * For each deck the test prints the error code and, for a run that went
 * ahead, the washoff load from the report's Runoff Quality Continuity table
 * and its ratio to the valid deck's load.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* runs a deck (swmm_run() without the console messages); returns the error code */
static int runDeck(const char *inp, const char *rpt, const char *out)
{
    double elapsed = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    return err;
}

/* washoff load (lbs) in the report's Runoff Quality Continuity table, or -1 */
static double readWashoff(const char *rpt)
{
    char line[256];
    int inTable = 0;
    double load = -1.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Runoff Quality Continuity")) inTable = 1;
        if (inTable && strstr(line, "Surface Runoff"))
        {
            char *p = strstr(line, ". ");
            while (p && *p == '.') p++;
            if (p) sscanf(p, "%lf", &load);
            break;
        }
    }
    fclose(f);
    return load;
}

int main(void)
{
    const char *decks[4] = {"split-100", "single-160", "total-130", "negative"};
    const char *covers[4] = {"60 + 40", "160", "60 + 70", "100 + (-50)"};
    char inp[64], rpt[64], out[64];
    int d, err[4], nAccepted = 0;
    double load[4], ref;

    for (d = 0; d < 4; d++)
    {
        snprintf(inp, sizeof inp, "IO-19_%s.inp", decks[d]);
        snprintf(rpt, sizeof rpt, "IO-19_%s.rpt", decks[d]);
        snprintf(out, sizeof out, "IO-19_%s.out", decks[d]);
        err[d] = runDeck(inp, rpt, out);
        load[d] = err[d] ? -1.0 : readWashoff(rpt);
    }
    ref = load[0];

    printf("TSS washoff from S1 (EMC 100 mg/L on every land use):\n");
    printf("  %-12s %-14s %6s %16s\n", "deck", "coverage (%)", "error", "washoff (lbs)");
    for (d = 0; d < 4; d++)
    {
        if (err[d]) printf("  %-12s %-14s %6d %16s\n", decks[d], covers[d], err[d], "rejected");
        else printf("  %-12s %-14s %6d %10.3f (%.2fx)\n", decks[d], covers[d], err[d],
                    load[d], ref > 0.0 ? load[d] / ref : 0.0);
        if (d > 0 && !err[d]) nAccepted++;
    }
    printf("  (correct: split-100 runs; the other three are rejected)\n");

    if (err[0] || ref <= 0.0)
    {
        printf("FAIL: the valid deck (60 %% + 40 %%) did not run (error %d)\n", err[0]);
        return 1;
    }
    if (nAccepted)
    {
        printf("FAIL: %d of 3 decks with invalid coverages ran without an error", nAccepted);
        if (!err[1]) printf(" (160 %% gives %.3f lbs, %.2fx the load of 100 %%)",
                            load[1], load[1] / ref);
        printf("\n");
        return 1;
    }
    printf("PASS: coverages outside 0-100 %% or above 100 %% in total are rejected\n");
    return 0;
}
