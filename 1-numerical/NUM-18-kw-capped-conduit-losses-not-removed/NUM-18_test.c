/*
 * NUM-18 (legacy toolkit API, 5.2.4 and 5.3.0): under STEADY and KINWAVE
 * routing, when a conduit's inflow is capped at its full flow, its seepage
 * (and evaporation) loss is booked in the mass balance but never taken out of
 * the water.
 *
 * J1 receives 40 cfs and drains through C1 (2 ft, 2000 ft at 0.5 %, about
 * 16 cfs when full) with 50 in/hr of seepage (about 4.6 cfs). C1 runs full:
 * it takes its full flow from J1, delivers its full flow to O1, and the
 * seepage that is reported as Exfiltration Loss comes from nowhere. The
 * decks differ only in FLOW_ROUTING.
 *
 * Correct behaviour: water is conserved, so the flow continuity error is
 * close to 0. The same deck with 5 cfs, which does not fill C1, gives
 * 0.000 % (STEADY) and -0.69 % (KINWAVE, which loses about 0.1 ac-ft while
 * the empty conduit fills). Tolerance 1 %; the bug gives about -12 %.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* reads a number from the "Flow Routing Continuity" table of a report */
static double rptValue(const char *rpt, const char *label)
{
    char line[256];
    int found = 0;
    double x = NAN;
    FILE *f = fopen(rpt, "r");
    if (!f) return NAN;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "Flow Routing Continuity")) found = 1;
        if (found && strstr(line, label))
        {
            char *p = strstr(line, label) + strlen(label);
            while (*p == ' ' || *p == '.') p++;
            x = strtod(p, NULL);
            break;
        }
    }
    fclose(f);
    return x;
}

static int runDeck(const char *inp, const char *rpt, const char *out, double *err)
{
    double t = 0.0;
    float e1, e2, e3;
    int rc = swmm_open(inp, rpt, out);
    if (!rc) rc = swmm_start(1);
    while (!rc)
    {
        rc = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_getMassBalErr(&e1, &e2, &e3);
    swmm_report();
    swmm_close();
    *err = e2;
    return rc;
}

int main(void)
{
    const char *decks[2] = {"NUM-18_steady.inp", "NUM-18_kinwave.inp"};
    const char *rpts[2]  = {"NUM-18_sf.rpt", "NUM-18_kw.rpt"};
    const char *outs[2]  = {"NUM-18_sf.out", "NUM-18_kw.out"};
    double err[2];
    int i, ok = 1;

    printf("%-20s %9s %9s %9s %9s %9s\n", "Deck", "Inflow", "Outflow", "Flooding",
           "Exfil.", "Error");
    printf("%-20s %9s %9s %9s %9s %9s\n", "", "(ac-ft)", "(ac-ft)", "(ac-ft)", "(ac-ft)", "(%)");
    for (i = 0; i < 2; i++)
    {
        if (runDeck(decks[i], rpts[i], outs[i], &err[i]))
        {
            printf("FAIL: %s stopped with an error\n", decks[i]);
            return 1;
        }
        printf("%-20s %9.3f %9.3f %9.3f %9.3f %9.3f\n", decks[i],
               rptValue(rpts[i], "External Inflow"), rptValue(rpts[i], "External Outflow"),
               rptValue(rpts[i], "Flooding Loss"), rptValue(rpts[i], "Exfiltration Loss"),
               err[i]);
        if (!(fabs(err[i]) <= 1.0)) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: the seepage of the full conduit is booked but not removed: flow "
               "continuity error %.3f %% (STEADY) and %.3f %% (KINWAVE)\n", err[0], err[1]);
        return 1;
    }
    printf("PASS: the full conduit's seepage is taken from the water it receives "
           "(continuity errors %.3f %% and %.3f %%)\n", err[0], err[1]);
    return 0;
}
