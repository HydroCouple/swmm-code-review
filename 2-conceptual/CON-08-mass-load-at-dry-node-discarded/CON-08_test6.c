/*
 * CON-08 for 6.0.0: a MASS-type pollutant load at a node that receives no
 * flow is booked as External Inflow and then discarded.
 *
 * 6.0.0 copies the legacy logic: the node CSTR branch of
 * QualitySolver::mixAtNodes() keeps the old concentration when the inflow
 * is at or below ZERO, and the non-storage branch never uses qual_mass_in
 * then, so the load reaches the ledger but no node.
 *
 * Decks and checks as in CON-08_test.c (DYNWAVE, 2 h, MASS load 10 mg/s):
 *   pond-mass-load          storage SU1 with 2000 ft3 of still water;
 *                           expected 72000 mg / (2000 x 28.317 L) = 1.2713 mg/L
 *   dry-junction-mass-load  junction J2 with no water at all
 * The quality continuity error, reported and recomputed from the table,
 * must be within 1 % (the bug gives 100 %), and SU1 within 1 % of 1.2713
 * mg/L (read with swmm_node_get_quality() after the last step).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

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

        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i], "CON-08_6.rpt", "CON-08_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        if (!rc && i == 0) su1 = swmm_node_index(e, "SU1");
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (rc || t <= 0.0) break;
            if (su1 >= 0) swmm_node_get_quality(e, su1, 0, &c);
        }
        if (!rc) rc = swmm_engine_end(e);
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);

        exIn = qual_line("CON-08_6.rpt", "External Inflow");
        fin  = qual_line("CON-08_6.rpt", "Final Stored Mass");
        in  = qual_line("CON-08_6.rpt", "Dry Weather Inflow") + qual_line("CON-08_6.rpt", "Wet Weather Inflow") +
              qual_line("CON-08_6.rpt", "Groundwater Inflow") + qual_line("CON-08_6.rpt", "RDII Inflow") +
              exIn + qual_line("CON-08_6.rpt", "Initial Stored Mass");
        out = qual_line("CON-08_6.rpt", "External Outflow") + qual_line("CON-08_6.rpt", "Flooding Loss") +
              qual_line("CON-08_6.rpt", "Exfiltration Loss") + qual_line("CON-08_6.rpt", "Mass Reacted") + fin;
        err = 100.0 * (in - out) / in;
        rep = qual_line("CON-08_6.rpt", "Continuity Error");

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
