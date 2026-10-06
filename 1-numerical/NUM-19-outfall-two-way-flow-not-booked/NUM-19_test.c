/*
 * NUM-19 (legacy toolkit API, 5.2.4 and 5.3.0): an outfall that both
 * receives and sends flow books nothing to the flow mass balance.
 *
 * In both decks a FIXED outfall OHI (stage 2 ft above its invert) feeds a
 * steady 13.7 cfs through J1 to the free outfall OLO:
 *   NUM-19_skip-steady.inp    with SKIP_STEADY_STATE YES (SYS_FLOW_TOL 1 %,
 *                             so skipping starts at steady state). On a skipped step
 *                             OHI still has the inflow that
 *                             node_getSystemOutflow() wrote on the step
 *                             before, as well as its outflow.
 *   NUM-19_outfall-inflow.inp with a 1 cfs FLOW inflow at OHI itself.
 * node_getSystemOutflow() returns 0 for an outfall whose inflow and outflow
 * are both nonzero, so the water OHI supplies is not booked.
 *
 * Correct behaviour: water is conserved. The same network without skipping
 * and without an inflow at OHI has a flow continuity error of -0.06 %, so the
 * test requires the error to be within 1 %. The bug gives -99 % and -1255 %.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* reads the number after a label in a report, after the line holding `after` */
static double rptValue(const char *rpt, const char *after, const char *label)
{
    char line[256];
    int found = 0;
    double x = NAN;
    FILE *f = fopen(rpt, "r");
    if (!f) return NAN;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, after)) found = 1;
        if (found && strstr(line, label))
        {
            char *p = strstr(line, label) + strlen(label);
            while (*p == ' ' || *p == '.' || *p == ':') p++;
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
    const char *decks[2] = {"NUM-19_skip-steady.inp", "NUM-19_outfall-inflow.inp"};
    const char *rpts[2]  = {"NUM-19_skip.rpt", "NUM-19_inflow.rpt"};
    const char *outs[2]  = {"NUM-19_skip.out", "NUM-19_inflow.out"};
    double err[2], in, out, steady;
    int i, ok = 1;

    printf("%-26s %10s %10s %10s %8s\n", "Deck", "Ext. in", "Ext. out", "Error", "Steady");
    printf("%-26s %10s %10s %10s %8s\n", "", "(ac-ft)", "(ac-ft)", "(%)", "(%)");
    for (i = 0; i < 2; i++)
    {
        if (runDeck(decks[i], rpts[i], outs[i], &err[i]))
        {
            printf("FAIL: %s stopped with an error\n", decks[i]);
            return 1;
        }
        in = rptValue(rpts[i], "Flow Routing Continuity", "External Inflow");
        out = rptValue(rpts[i], "Flow Routing Continuity", "External Outflow");
        steady = rptValue(rpts[i], "Routing Time Step Summary", "% of Time in Steady State");
        printf("%-26s %10.3f %10.3f %10.3f %8.2f\n", decks[i], in, out, err[i], steady);
        if (!(fabs(err[i]) <= 1.0)) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: water supplied by the outfall is not booked: flow continuity error "
               "%.3f %% with SKIP_STEADY_STATE and %.3f %% with an inflow at the outfall\n",
               err[0], err[1]);
        return 1;
    }
    printf("PASS: the outfall's net flow is booked (continuity errors %.3f %% and %.3f %%)\n",
           err[0], err[1]);
    return 0;
}
