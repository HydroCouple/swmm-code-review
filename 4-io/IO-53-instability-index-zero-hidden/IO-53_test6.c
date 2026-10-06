/*
 * IO-53 for 6.0.0: DefaultReportPlugin reproduces the legacy test (see
 * IO-53_test.c), so the same checks apply.
 *
 * Legacy: "All links are stable." and "Convergence obtained at all time
 * steps." are printed when the worst link or node is the first one in the
 * input file.
 *
 * report_writeMaxFlowTurns() and report_writeNonconvergedStats() test the
 * top entry of their ranking with `index <= 0`. An empty ranking has index
 * -1 (stats_findMaxStats() initialises it so), and 0 is the first link or
 * node of the model. When that element is the worst one, the whole list is
 * replaced by the "nothing to report" sentence.
 *
 * The two decks hold the same model, J1 -> C1 -> J2 -> C2 -> O1, with an
 * inflow to J1 that alternates between 1 and 3 cfs at every 30 s routing
 * step, MAX_TRIALS 1 and HEAD_TOLERANCE 0.0001 ft: the conduit flows keep
 * rising and falling and the nodes often fail to converge. Only the order of
 * the [JUNCTIONS], [CONDUITS] and [XSECTIONS] entries differs.
 *
 * Correct behaviour, checked against independent evidence for each deck:
 *  - the test counts the flow turns of each conduit from the flows it reads
 *    at every step (a turn is a change in the sign of the step-to-step flow
 *    change larger than 0.001 cfs, the rule of stats_updateLinkStats()).
 *    If a conduit turns at 5 % or more of the steps, the report must not
 *    say "All links are stable." (the deck gives 9 % to 30 %);
 *  - if the report's own "% of Steps Not Converging" is above 0.005 %
 *    (the threshold of the nonconvergence list), it must not say
 *    "Convergence obtained at all time steps." (the deck gives 20 % to 65 %).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

#define MAXLINKS 8

/* Prints the lines of the report section that follows `title` and returns
 * 1 if one of them contains `none`. */
static int section(const char *rpt, const char *title, const char *none)
{
    char line[256];
    int found = 0, n = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f) && !strstr(line, title)) ;
    fgets(line, sizeof line, f);                     /* the *** line */
    while (fgets(line, sizeof line, f) && n < 6)
    {
        if (line[strspn(line, " \r\n")] == '\0') { if (n) break; else continue; }
        printf("    | %s", line);
        if (strstr(line, none)) found = 1;
        n++;
    }
    fclose(f);
    return found;
}

/* Returns the report's "% of Steps Not Converging" (-1 if absent). */
static double pctNotConverging(const char *rpt)
{
    char line[256];
    double x = -1.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return x;
    while (fgets(line, sizeof line, f))
    {
        char *p = strstr(line, "% of Steps Not Converging");
        if (p && (p = strchr(p, ':'))) x = atof(p + 1);
    }
    fclose(f);
    return x;
}

/* Runs one deck; returns 1 if its report is consistent. */
static int runDeck(const char *inp, const char *rpt, const char *out)
{
    double t = 0.0, q, dq, maxFrac = 0.0, notConv;
    double qOld[MAXLINKS] = {0};
    int sign[MAXLINKS] = {0}, turns[MAXLINKS] = {0};
    int rc, j, s, nLinks = 0, steps = 0, stable, converged, ok = 1;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    if (!rc) nLinks = swmm_link_count(e);
    if (nLinks > MAXLINKS) nLinks = MAXLINKS;
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (rc || t <= 0) break;
        steps++;
        for (j = 0; j < nLinks; j++)
        {
            swmm_link_get_flow(e, j, &q);
            dq = q - qOld[j];
            s = (dq > 0.0) - (dq < 0.0);
            if (fabs(dq) > 0.001 && s * sign[j] < 0) turns[j]++;
            sign[j] = s;
            qOld[j] = q;
        }
    }
    printf("%s (%d routing steps)\n", inp, steps);
    for (j = 0; j < nLinks; j++)
    {
        int c1 = swmm_link_index(e, "C1");
        printf("    link %d = %s: flow turned %d times (%.1f %% of steps)\n",
               j, j == c1 ? "C1" : "C2", turns[j], 100.0 * turns[j] / steps);
        if ((double)turns[j] / steps > maxFrac) maxFrac = (double)turns[j] / steps;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("    run stopped with error %d\n", rc); return 0; }

    printf("  Highest Flow Instability Indexes:\n");
    stable = section(rpt, "Highest Flow Instability Indexes", "All links are stable.");
    notConv = pctNotConverging(rpt);
    printf("  Most Frequent Nonconverging Nodes (%% of Steps Not Converging = %.2f):\n", notConv);
    converged = section(rpt, "Most Frequent Nonconverging Nodes",
                        "Convergence obtained at all time steps.");

    /* 5 %: the deck's conduits turn at 9 % to 30 % of the steps;
     * a stable conduit turns at none */
    if (stable && maxFrac >= 0.05)
    {
        printf("  -> wrong: a conduit turned at %.1f %% of the steps but the report "
               "says all links are stable\n", 100.0 * maxFrac);
        ok = 0;
    }
    if (converged && notConv > 0.005)
    {
        printf("  -> wrong: %.2f %% of the steps did not converge but the report "
               "says convergence was obtained at all time steps\n", notConv);
        ok = 0;
    }
    printf("\n");
    return ok;
}

int main(void)
{
    int ok1 = runDeck("IO-53_c1-first.inp", "IO-53a6.rpt", "IO-53a6.out");
    int ok2 = runDeck("IO-53_c2-first.inp", "IO-53b6.rpt", "IO-53b6.out");
    if (!ok1 || !ok2)
    {
        printf("FAIL: the report says the links are stable or the nodes converged "
               "although they are not (%s)\n",
               !ok1 && !ok2 ? "both decks" : !ok1 ? "IO-53_c1-first.inp" : "IO-53_c2-first.inp");
        return 1;
    }
    printf("PASS: both reports list the unstable links and nonconverging nodes, "
           "whatever their order in the input\n");
    return 0;
}
