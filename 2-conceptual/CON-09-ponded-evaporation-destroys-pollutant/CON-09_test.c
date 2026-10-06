/*
 * CON-09: pollutant carried by water ponded on a subcatchment surface
 * disappears when that water evaporates.
 *
 * findPondedLoads() mixes the ponded mass with rain and run-on,
 * cPonded = wPonded / Vinflow, removes the infiltrated and runoff mass,
 * and then rebuilds the ponded mass from the new depth:
 * pondedQual = cPonded * depth * area. Since Vinflow = Vinfil + Voutflow
 * + Vevap + Vfinal, the mass cPonded * Vevap - the pollutant that was in the
 * evaporated water - is dropped, with no entry in the runoff quality
 * ledger. Evaporation removes water, not solute.
 *
 * Decks (10 ac impervious, 0.2 in depression storage, rain at 10 mg/L,
 * evaporation 0.5 in/day, 24 h):
 *   rain-evaporates  0.1 in of rain, nothing runs off, all of it evaporates
 *   two-storms       0.1 in that evaporates, then 0.3 in that runs off
 *
 * Correct behaviour (conservation of mass): every pound deposited by rain
 * ends in Surface Runoff or Remaining Buildup. The test requires the runoff
 * quality continuity error, as reported and as recomputed from the table's
 * own lines, within 1 % (the bug gives 100 % and 82 %).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

static const char *decks[] = {"CON-09_rain-evaporates.inp", "CON-09_two-storms.inp"};

/* First-pollutant value of a line of the Runoff Quality Continuity table */
static double qual_line(const char *rpt, const char *label)
{
    FILE *f = fopen(rpt, "r");
    char line[512];
    int inTable = 0;
    double v = 0.0;
    if (!f) return NAN;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Runoff Quality Continuity")) inTable = 1;
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
    printf("Deck                         Wet         Surface   Remaining  Error from  Reported\n");
    printf("                             deposition  runoff    buildup    the table   error\n");
    printf("                             (lb)        (lb)      (lb)       (%%)         (%%)\n");
    for (i = 0; i < 2; i++)
    {
        double t = 0.0, in, out, err, rep, dep, run, rem;
        int rc, bad;

        rc = swmm_open(decks[i], "CON-09.rpt", "CON-09.out");
        if (!rc) rc = swmm_start(1);
        while (!rc)
        {
            rc = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_report();
        swmm_close();

        dep = qual_line("CON-09.rpt", "Wet Deposition");
        run = qual_line("CON-09.rpt", "Surface Runoff");
        rem = qual_line("CON-09.rpt", "Remaining Buildup");
        in  = qual_line("CON-09.rpt", "Initial Buildup") + qual_line("CON-09.rpt", "Surface Buildup") + dep;
        out = qual_line("CON-09.rpt", "Sweeping Removal") + qual_line("CON-09.rpt", "Infiltration Loss") +
              qual_line("CON-09.rpt", "BMP Removal") + run + rem;
        err = 100.0 * (in - out) / in;
        rep = qual_line("CON-09.rpt", "Continuity Error");

        bad = rc || !(fabs(err) < 1.0) || !(fabs(rep) < 1.0);
        if (bad) nbad++;
        printf("%-27s  %9.3f  %8.3f  %9.3f  %10.3f  %8.3f%s\n", decks[i], dep, run, rem, err, rep,
               bad ? "  <-- wrong" : "");
        if (rc) printf("    run stopped with error %d\n", rc);
    }
    if (nbad)
    {
        printf("FAIL: in %d of 2 decks pollutant deposited in ponded water vanishes as the water "
               "evaporates (runoff quality continuity error)\n", nbad);
        return 1;
    }
    printf("PASS: pollutant in evaporating ponded water stays on the surface; the runoff quality balance closes\n");
    return 0;
}
