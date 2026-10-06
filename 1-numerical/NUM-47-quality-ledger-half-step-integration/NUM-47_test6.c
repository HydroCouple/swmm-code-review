/*
 * NUM-47 for 6.0.0: losses booked as step-averaged rates are integrated over
 * (dt_n + dt_n+1)/2 instead of dt_n in 5.2.4 and 5.3.0.
 *
 * 6.0.0 books each step's reaction loss as a mass ((c1 - c2) x volume) and
 * its flow terms as rate x dt of that step, so it does not have this defect;
 * the test is expected to pass unpatched.
 *
 * Deck: NUM-47_fast-decay.inp - a closed 4000 ft3 pond at 10 mg/L
 * (2.495 lb), first-order decay K1 = 1440/day, DYNWAVE with the default
 * variable step (first step 0.5 s, then 30 s). In 5.2.4/5.3.0 the decay of
 * step 1 is booked (0.5 + 30)/(2 x 0.5) = 30.5 times, and Mass Reacted
 * comes out 25 % too high.
 *
 * The treatment deck of NUM-47_test.c is not run here: 6.0.0 has a separate
 * defect in its treatment ledger (it adds the treatment removal RATE to the
 * reacted MASS total), which would fail the check for another reason.
 *
 * Correct behaviour (conservation of mass): Mass Reacted + Final Stored Mass
 * = Initial Stored Mass. The test requires the ratio within 2 % of 1 and the
 * reported continuity error within 2 % (the bug gives 1.25 and -24.6 %).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

static const char *decks[] = {"NUM-47_fast-decay.inp"};

/* First-pollutant value of a line of the Quality Routing Continuity table */
static double qual_line(const char *rpt, const char *label)
{
    FILE *f = fopen(rpt, "r");
    char line[512];
    int inTable = 0;
    double v = NAN;
    if (!f) return NAN;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Quality Routing Continuity")) inTable = 1;
        if (inTable && strstr(line, label))
        {
            char *p = strstr(line, "..");   /* skip the dot leader */
            if (p) { while (*p == '.') p++; sscanf(p, "%lf", &v); }
            break;
        }
    }
    fclose(f);
    return v;
}

int main(void)
{
    int i, nbad = 0;
    printf("Deck                              Initial   Mass      Final     (Reacted+Final)  Reported\n");
    printf("                                  stored    reacted   stored    / Initial        error (%%)\n");
    for (i = 0; i < 1; i++)
    {
        double t = 0.0, init, react, fin, err, ratio;
        int rc, bad;

        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i], "NUM-47_6.rpt", "NUM-47_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (rc || t <= 0.0) break;
        }
        if (!rc) rc = swmm_engine_end(e);
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);

        init  = qual_line("NUM-47_6.rpt", "Initial Stored Mass");
        react = qual_line("NUM-47_6.rpt", "Mass Reacted");
        fin   = qual_line("NUM-47_6.rpt", "Final Stored Mass");
        err   = qual_line("NUM-47_6.rpt", "Continuity Error");
        ratio = (react + fin) / init;
        bad = rc || !(fabs(ratio - 1.0) < 0.02) || !(fabs(err) < 2.0);
        if (bad) nbad++;
        printf("%-32s  %7.3f   %7.3f   %7.3f   %12.3f     %9.3f%s\n", decks[i], init, react, fin,
               ratio, err, bad ? "  <-- wrong" : "");
        if (rc) printf("    run stopped with error %d\n", rc);
    }
    if (nbad)
    {
        printf("FAIL: in %d of 1 deck Mass Reacted exceeds the mass that was there "
               "(the first 0.5 s step is booked with the next step's length)\n", nbad);
        return 1;
    }
    printf("PASS: Mass Reacted + Final Stored Mass = Initial Stored Mass\n");
    return 0;
}
