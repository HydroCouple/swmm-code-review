/*
 * CON-05: the rain falling on a covered rain barrel disappears when it has
 * nowhere to go.
 *
 * S1 is 1 ac (43,560 ft2) with no infiltration, evaporation or seepage and
 * 2.0 in of rain. It holds 100 covered rain barrels of 50 ft2 (5,000 ft2, 11.5 %
 * of S1; no drain, FromImp = 0):
 *   CON-05_pervious.inp  S1 is half pervious
 *   CON-05_imperv.inp    S1 is 100 % impervious
 *   CON-05_full-lid.inp  a 38,560 ft2 rain garden and the barrels fill S1
 * No water leaves except as runoff, so by continuity
 *
 *     runoff + final storage = rain + initial LID storage
 *
 * and a covered barrel receives no water itself (Total Inflow 0 in the LID
 * Performance Summary): the rain on its top has to run off somewhere. 5.2.4
 * drops it in all three decks; 5.3.0 returns it to the pervious area and drops
 * it when there is none (imperv, full-lid). Either way 11.5 % of the rain is
 * lost (a continuity error of about +11 %). The limit of 1 % is 10 times smaller than
 * that and 100 times larger than the error of a correct run (0.00 %).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

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

/* Total Inflow (3rd column) of the LID Performance Summary row for this LID */
static int lidInflow(const char* rpt, const char* lid, double* v)
{
    char line[512], a[64], b[64];
    int inTable = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "LID Performance Summary")) inTable = 1;
        if (!inTable) continue;
        /* 6.0.0 prints this instead of the table when no unit had any inflow */
        if (strstr(line, "No LID performance data"))
        {
            *v = 0.0;
            fclose(f);
            return 1;
        }
        if (sscanf(line, "%63s %63s %lf", a, b, v) == 3 &&
            (strcmp(a, lid) == 0 || strcmp(b, lid) == 0))
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main(void)
{
    const char* name[3] = {"pervious", "imperv", "full-lid"};
    const char* desc[3] = {"half pervious", "100 % impervious", "LIDs fill S1"};
    int k, bad = 0, notCovered = 0;

    printf("S1                      rain    init LID  runoff  final    balance   barrel\n");
    printf("                       (ac-ft)  (ac-ft)  (ac-ft)  (ac-ft)  error (%%) inflow (in)\n");
    for (k = 0; k < 3; k++)
    {
        char inp[64], rpt[64], out[64];
        double elapsed = 0.0, rain = 0, init = 0, evap = 0, infil = 0,
               runoff = 0, store = 0, rbIn = 0, err;
        int rc;

        sprintf(inp, "CON-05_%s.inp", name[k]);
        sprintf(rpt, "CON-05_%s.rpt", name[k]);
        sprintf(out, "CON-05_%s.out", name[k]);
        rc = swmm_open(inp, rpt, out);
        if (!rc) rc = swmm_start(1);
        while (!rc)
        {
            rc = swmm_step(&elapsed);
            if (elapsed <= 0.0) break;
        }
        if (!rc) rc = swmm_end();
        if (!rc) rc = swmm_report();
        swmm_close();
        /* the report has no "Initial LID Storage" line when it is zero */
        if (!rc && !rptValue(rpt, "Initial LID Storage", &init)) init = 0.0;
        if (rc ||
            !rptValue(rpt, "Total Precipitation", &rain) ||
            !rptValue(rpt, "Evaporation Loss", &evap) ||
            !rptValue(rpt, "Infiltration Loss", &infil) ||
            !rptValue(rpt, "Surface Runoff", &runoff) ||
            !rptValue(rpt, "Final Storage", &store) ||
            !lidInflow(rpt, "RB1", &rbIn))
        {
            printf("FAIL: the run of %s did not complete (error %d)\n", inp, rc);
            return 1;
        }
        err = 100.0 * ((rain + init) - (evap + infil + runoff + store)) / (rain + init);
        printf("%-22s %6.3f   %6.3f   %6.3f   %6.3f   %8.3f   %6.2f\n",
               desc[k], rain, init, runoff, store, err, rbIn);
        if (fabs(err) > 1.0) bad++;
        if (rbIn > 0.1) notCovered++;
    }
    if (bad)
    {
        printf("FAIL: S1's runoff and storage do not balance the rain for %d of 3 "
               "decks (more than 1 %% off); the rain on the covered barrels is lost\n", bad);
        return 1;
    }
    if (notCovered)
    {
        printf("FAIL: the barrels declared covered (Covrd YES) collect rain\n");
        return 1;
    }
    printf("PASS: runoff + storage = rain within 1 %% and the covered barrels take no rain, "
           "with and without a pervious area\n");
    return 0;
}
