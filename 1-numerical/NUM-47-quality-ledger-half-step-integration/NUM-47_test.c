/*
 * NUM-47: losses booked as step-averaged rates (treatment, first-order
 * decay, evaporation, seepage) are integrated over (dt_n + dt_n+1)/2
 * instead of over their own step dt_n.
 *
 * routing_execute() updates the run totals with the step totals twice:
 * massbal_updateRoutingTotals(routingStep/2) at the end of step n, and again
 * at the START of step n+1, before the step totals are cleared - with step
 * n+1's length. For a flow sampled at the end of step n that is the
 * trapezoidal rule. But treatment and decay book (mass removed in step n) /
 * dt_n, so the mass booked is (removed mass) x (dt_n + dt_n+1)/(2 dt_n).
 * Under DYNWAVE with a variable step the first step is 0.5 s and the next
 * the full routing step, so whatever is removed in step 1 is booked
 * (0.5 + 10)/(2 x 0.5) = 10.5 times with a 10 s routing step.
 *
 * Decks (closed 4000 ft3 pond, 10 mg/L = 2.495 lb, nothing enters or leaves,
 * DYNWAVE with the default variable step):
 *   treat-initial-store  treatment TP C = 0 removes the stored TP in step 1
 *   fast-decay           first-order decay, K1 = 1440/day, routing step 30 s
 *
 * Correct behaviour (conservation of mass): Mass Reacted + Final Stored Mass
 * = Initial Stored Mass, so the quality continuity error is about 0. The
 * test requires (reacted + final) / initial within 2 % of 1 and the reported
 * continuity error within 2 % (the bug gives 10.5 and 1.25: -950 % and
 * -24.6 %).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

static const char *decks[] = {"NUM-47_treat-initial-store.inp", "NUM-47_fast-decay.inp"};

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
    for (i = 0; i < 2; i++)
    {
        double t = 0.0, init, react, fin, err, ratio;
        int rc, bad;

        rc = swmm_open(decks[i], "NUM-47.rpt", "NUM-47.out");
        if (!rc) rc = swmm_start(1);
        while (!rc)
        {
            rc = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_report();
        swmm_close();

        init  = qual_line("NUM-47.rpt", "Initial Stored Mass");
        react = qual_line("NUM-47.rpt", "Mass Reacted");
        fin   = qual_line("NUM-47.rpt", "Final Stored Mass");
        err   = qual_line("NUM-47.rpt", "Continuity Error");
        ratio = (react + fin) / init;
        bad = rc || !(fabs(ratio - 1.0) < 0.02) || !(fabs(err) < 2.0);
        if (bad) nbad++;
        printf("%-32s  %7.3f   %7.3f   %7.3f   %12.3f     %9.3f%s\n", decks[i], init, react, fin,
               ratio, err, bad ? "  <-- wrong" : "");
        if (rc) printf("    run stopped with error %d\n", rc);
    }
    if (nbad)
    {
        printf("FAIL: in %d of 2 decks Mass Reacted exceeds the mass that was there "
               "(the first 0.5 s step is booked with the next step's length)\n", nbad);
        return 1;
    }
    printf("PASS: Mass Reacted + Final Stored Mass = Initial Stored Mass in both decks\n");
    return 0;
}
