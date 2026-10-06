/*
 * CON-09 for 6.0.0: pollutant carried by water ponded on a subcatchment
 * surface disappears when that water evaporates.
 *
 * 6.0.0 rebuilds the ponded mass from the new depth as 5.3.0 does
 * (ponded_qual = c_ponded * depth * area), so the pollutant in the
 * evaporated water is dropped. In addition, when the surface dries it sets
 * the ponded mass to 0 without booking it, and its Remaining Buildup leaves
 * out the mass still ponded at the end of the run.
 *
 * Decks and checks as in CON-09_test.c (10 ac impervious, 0.2 in
 * depression storage, rain at 10 mg/L, evaporation 0.5 in/day):
 *   rain-evaporates  0.1 in of rain that all evaporates
 *   two-storms       0.1 in that evaporates, then 0.3 in that runs off
 * The runoff quality continuity error, reported and recomputed from the
 * table, must be within 1 % (the bug gives 100 % and 88 %).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

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

        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i], "CON-09_6.rpt", "CON-09_6.out", NULL);
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

        dep = qual_line("CON-09_6.rpt", "Wet Deposition");
        run = qual_line("CON-09_6.rpt", "Surface Runoff");
        rem = qual_line("CON-09_6.rpt", "Remaining Buildup");
        in  = qual_line("CON-09_6.rpt", "Initial Buildup") + qual_line("CON-09_6.rpt", "Surface Buildup") + dep;
        out = qual_line("CON-09_6.rpt", "Sweeping Removal") + qual_line("CON-09_6.rpt", "Infiltration Loss") +
              qual_line("CON-09_6.rpt", "BMP Removal") + run + rem;
        err = 100.0 * (in - out) / in;
        rep = qual_line("CON-09_6.rpt", "Continuity Error");

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
