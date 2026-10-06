/*
 * CON-08: a MASS-type pollutant load at a node that receives no flow is
 * booked as External Inflow and then discarded.
 *
 * addExternalInflows() adds a MASS inflow (and the 5.3.0 API mass flux) to
 * the node's mass accumulator and to the External Inflow ledger, whether or
 * not the node has any flow. The mixing code then ignores the accumulator
 * when the node's inflow is at or below ZERO (1e-10 cfs): getMixedQual()
 * returns the old concentration of a storage unit, and findNodeQual() keeps
 * the old (or, in 5.2.4, zero) concentration of a junction. The mass goes
 * nowhere, so the continuity error is 100 %.
 *
 * Decks (DYNWAVE, 2 h, MASS load 10 mg/s of P1 = 72000 mg = 0.159 lb, no flow):
 *   pond-mass-load          into storage SU1 holding 2000 ft3 of still water;
 *                           expected concentration 72000 mg / (2000 ft3 x
 *                           28.317 L/ft3) = 1.2713 mg/L
 *   dry-junction-mass-load  into junction J2, which has no water at all
 *
 * Correct behaviour (conservation of mass): the load ends up somewhere the
 * mass balance can see. The test requires the quality continuity error,
 * both as reported and as recomputed from the table's own lines, within 1 %
 * (the bug gives 100 %), and for the pond the SU1 concentration within 1 %
 * of 1.2713 mg/L (the bug leaves it at 0).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

static const char *decks[] = {"CON-08_pond-mass-load.inp", "CON-08_dry-junction-mass-load.inp"};

/* First-pollutant value of a line of the Quality Routing Continuity table */
static double qual_line(const char *rpt, const char *label)
{
    FILE *f = fopen(rpt, "r");
    char line[512];
    int inTable = 0;
    double v = 0.0;
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
    const double cExpected = 10.0 * 7200.0 / (2000.0 * 28.317);   /* mg/L */
    int i, nbad = 0;

    printf("Deck                                 External  Final    Error from  Reported   SU1 P1 at end\n");
    printf("                                     inflow    stored   the table   error      (mg/L, expected\n");
    printf("                                     (lb)      (lb)     (%%)         (%%)        %.4f)\n", cExpected);
    for (i = 0; i < 2; i++)
    {
        double t = 0.0, in, out, err, rep, exIn, fin, c = NAN;
        int rc, su1 = -1, bad;

        rc = swmm_open(decks[i], "CON-08.rpt", "CON-08.out");
        if (!rc) rc = swmm_start(1);
        if (!rc && i == 0) su1 = swmm_getIndex(swmm_NODE, "SU1");
        while (!rc)
        {
            rc = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_report();
        swmm_close();

        if (su1 >= 0)
        {
            SMO_Handle h = NULL;
            float *v = NULL;
            int len = 0, nper = 0;
            SMO_init(&h);
            /* node results: depth, head, volume, lat. inflow, inflow, flooding, P1 */
            if (SMO_open(h, "CON-08.out") == 0 && SMO_getTimes(h, SMO_numPeriods, &nper) == 0 &&
                SMO_getNodeResult(h, nper - 1, su1, &v, &len) == 0 && len > 6)
                c = v[6];
            SMO_free((void **)&v);
            SMO_close(&h);
        }

        exIn = qual_line("CON-08.rpt", "External Inflow");
        fin  = qual_line("CON-08.rpt", "Final Stored Mass");
        in  = qual_line("CON-08.rpt", "Dry Weather Inflow") + qual_line("CON-08.rpt", "Wet Weather Inflow") +
              qual_line("CON-08.rpt", "Groundwater Inflow") + qual_line("CON-08.rpt", "RDII Inflow") +
              exIn + qual_line("CON-08.rpt", "Initial Stored Mass");
        out = qual_line("CON-08.rpt", "External Outflow") + qual_line("CON-08.rpt", "Flooding Loss") +
              qual_line("CON-08.rpt", "Exfiltration Loss") + qual_line("CON-08.rpt", "Mass Reacted") + fin;
        err = 100.0 * (in - out) / in;
        rep = qual_line("CON-08.rpt", "Continuity Error");

        bad = rc || !(fabs(err) < 1.0) || !(fabs(rep) < 1.0) ||
              (i == 0 && !(fabs(c - cExpected) < 0.01 * cExpected));
        if (bad) nbad++;
        if (i == 0)
            printf("%-35s  %8.3f  %7.3f  %10.3f  %8.3f   %9.4f%s\n", decks[i], exIn, fin, err, rep, c,
                   bad ? "  <-- wrong" : "");
        else
            printf("%-35s  %8.3f  %7.3f  %10.3f  %8.3f         n/a%s\n", decks[i], exIn, fin, err, rep,
                   bad ? "  <-- wrong" : "");
        if (rc) printf("    run stopped with error %d\n", rc);
    }
    if (nbad)
    {
        printf("FAIL: in %d of 2 decks the MASS load is booked as inflow but goes nowhere "
               "(continuity error 100 %%, pond concentration not raised)\n", nbad);
        return 1;
    }
    printf("PASS: a MASS load at a node with no flow is kept and the quality mass balance closes\n");
    return 0;
}
