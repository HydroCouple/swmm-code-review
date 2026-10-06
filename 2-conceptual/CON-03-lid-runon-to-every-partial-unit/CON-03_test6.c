/*
 * CON-03 for 6.0.0: upstream run-on is added to every LID unit, also when the LID covers
 * only part of the subcatchment.
 *
 * S1 (10 ac, impervious) drains onto S2 (10 ac, pervious, no infiltration).
 * 2.0 in of rain falls on both. S2 holds a rain garden with no seepage:
 *   CON-03_partial-lid.inp  rain garden = 5 ac (half of S2)
 *   CON-03_full-lid.inp     rain garden = 10 ac (all of S2)
 * No water leaves the system except as S2's runoff, so by continuity
 *
 *     runoff + final storage = rain + initial LID storage
 *
 * and the runoff can never exceed the rain. The test reads the Runoff Quantity
 * Continuity table of each run's report and computes the balance itself.
 * With the bug, the partial-LID run creates water: S1's run-on is spread over
 * S2's non-LID area and ALSO added to the rain garden's inflow, so S2 sends
 * out 4.8 ac-ft from 3.3 ac-ft of rain (-49 %). The limit of 1 % is 50 times
 * smaller than that error and 100 times larger than the error of a correct
 * run (-0.007 %).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* first number after the dotted leader of the first report line containing label */
static int rptValue(const char* rpt, const char* label, double* v)
{
    char line[512];
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, label);
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        fclose(f);
        return sscanf(p, "%lf", v) == 1;
    }
    fclose(f);
    return 0;
}

int main(void)
{
    const char* name[2] = {"partial", "full"};
    const char* desc[2] = {"5 ac rain garden", "10 ac rain garden"};
    int k, bad = 0;

    printf("S2 LID              rain    init LID  runoff  final    balance\n");
    printf("                   (ac-ft)  (ac-ft)  (ac-ft)  (ac-ft)  error (%%)\n");
    for (k = 0; k < 2; k++)
    {
        char inp[64], rpt[64], out[64];
        SWMM_Engine e;
        double elapsed = 0.0, rain = 0, init = 0, evap = 0, infil = 0,
               runoff = 0, store = 0, err;
        int rc;

        sprintf(inp, "CON-03_%s-lid.inp", name[k]);
        sprintf(rpt, "CON-03_%s-lid6.rpt", name[k]);
        sprintf(out, "CON-03_%s-lid6.out", name[k]);
        e = swmm_engine_create();
        rc = swmm_engine_open(e, inp, rpt, out, NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        while (!rc)
        {
            rc = swmm_engine_step(e, &elapsed);
            if (elapsed <= 0.0) break;
        }
        if (!rc) rc = swmm_engine_end(e);
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        if (rc ||
            !rptValue(rpt, "Initial LID Storage", &init) ||
            !rptValue(rpt, "Total Precipitation", &rain) ||
            !rptValue(rpt, "Evaporation Loss", &evap) ||
            !rptValue(rpt, "Infiltration Loss", &infil) ||
            !rptValue(rpt, "Surface Runoff", &runoff) ||
            !rptValue(rpt, "Final Storage", &store))
        {
            printf("FAIL: the run of %s did not complete (error %d)\n", inp, rc);
            return 1;
        }
        err = 100.0 * ((rain + init) - (evap + infil + runoff + store)) / (rain + init);
        printf("%-18s %6.3f   %6.3f   %6.3f   %6.3f   %8.3f\n",
               desc[k], rain, init, runoff, store, err);
        if (fabs(err) > 1.0 || runoff > rain + init) bad++;
    }
    if (bad)
    {
        printf("FAIL: S2's runoff and storage do not balance the rain for %d of 2 "
               "decks (more than 1 %% off); run-on onto a partial LID is counted twice\n", bad);
        return 1;
    }
    printf("PASS: runoff + storage = rain within 1 %% with a partial and a full LID\n");
    return 0;
}
