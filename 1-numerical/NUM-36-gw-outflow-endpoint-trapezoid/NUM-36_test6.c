/*
 * NUM-36 for 6.0.0: the groundwater flow sent to the drainage system is the
 * trapezoid of end-of-step rates (starting from 0), not the volume the
 * aquifer lost.
 *
 * NUM-36_recession.inp: a 10-acre aquifer drains to node J1 for 4 days with
 * no rain, no evaporation and no deep percolation. Lateral GW flow is the
 * only flux out of the aquifer, so by continuity
 *
 *     GW inflow received by J1 = initial - final aquifer storage.
 *
 * The test runs the deck with DRY_STEP 0:15, 1:00, 6:00 and 24:00 (copies
 * written next to the deck) and reads three numbers from each report:
 * Initial and Final Storage of the Groundwater Continuity table and
 * Groundwater Inflow of the Flow Routing Continuity table (acre-feet).
 *
 * Tolerance 0.25 %: the report prints 3 decimals of about 11 ac-ft
 * (0.005 %); with the bug the differences are -0.7 % (0:15), -2.7 % (1:00),
 * -1.6 % (6:00) and +13.7 % (24:00).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

#define TOL_PCT 0.25

/* first number after the dotted leader of the first line containing label
   that comes after the line containing section (section may be NULL) */
static int rptValue(const char* rpt, const char* section, const char* label,
                    double* v)
{
    char line[512];
    int inSection = (section == NULL);
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p;
        if (!inSection)
        {
            if (strstr(line, section)) inSection = 1;
            continue;
        }
        p = strstr(line, label);
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        fclose(f);
        return sscanf(p, "%lf", v) == 1;
    }
    fclose(f);
    return 0;
}

/* writes a copy of the base deck with the given DRY_STEP */
static int writeDeck(const char* base, const char* inp, const char* step)
{
    char line[512];
    FILE* in = fopen(base, "r");
    FILE* out = fopen(inp, "w");
    if (!in || !out)
    {
        if (in) fclose(in);
        if (out) fclose(out);
        return 0;
    }
    while (fgets(line, sizeof(line), in))
    {
        if (strncmp(line, "DRY_STEP", 8) == 0)
            fprintf(out, "DRY_STEP             %s\n", step);
        else fputs(line, out);
    }
    fclose(in);
    fclose(out);
    return 1;
}

static int runDeck(const char* inp, const char* rpt, const char* out)
{
    double t = 0.0;
    SWMM_Engine e = swmm_engine_create();
    int err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    const char* steps[] = {"00:15:00", "01:00:00", "06:00:00", "24:00:00"};
    const char* tags[]  = {"0015", "0100", "0600", "2400"};
    double worst = 0.0;
    const char* worstStep = "";
    int i;

    printf("DRY_STEP   Aquifer storage   GW inflow to J1   Difference   GW continuity\n");
    printf("             lost (ac-ft)          (ac-ft)          (%%)       error (%%)\n");
    for (i = 0; i < 4; i++)
    {
        char inp[64], rpt[64], out[64];
        double s0, s1, qgw, cerr, lost, diff;
        int err;
        sprintf(inp, "NUM-36_dry6_%s.inp", tags[i]);
        sprintf(rpt, "NUM-36_dry6_%s.rpt", tags[i]);
        sprintf(out, "NUM-36_dry6_%s.out", tags[i]);
        if (!writeDeck("NUM-36_recession.inp", inp, steps[i]))
        {
            printf("FAIL: could not write %s\n", inp);
            return 1;
        }
        err = runDeck(inp, rpt, out);
        if (err)
        {
            printf("FAIL: %s stopped with error %d\n", inp, err);
            return 1;
        }
        if (!rptValue(rpt, "Groundwater Continuity", "Initial Storage", &s0) ||
            !rptValue(rpt, "Groundwater Continuity", "Final Storage", &s1) ||
            !rptValue(rpt, "Groundwater Continuity", "Continuity Error (%)", &cerr) ||
            !rptValue(rpt, "Flow Routing Continuity", "Groundwater Inflow", &qgw))
        {
            printf("FAIL: could not read the continuity tables of %s\n", rpt);
            return 1;
        }
        lost = s0 - s1;
        diff = 100.0 * (qgw - lost) / lost;
        printf("%s   %15.3f   %15.3f   %10.2f   %13.3f\n",
               steps[i], lost, qgw, diff, cerr);
        if (fabs(diff) > fabs(worst))
        {
            worst = diff;
            worstStep = steps[i];
        }
    }

    if (fabs(worst) > TOL_PCT)
    {
        printf("FAIL: GW inflow to J1 differs from the aquifer's storage loss by "
               "up to %+.2f %% (DRY_STEP %s)\n", worst, worstStep);
        return 1;
    }
    printf("PASS: GW inflow to J1 equals the aquifer's storage loss within "
           "%.2f %% for every DRY_STEP (worst %+.3f %%)\n", TOL_PCT, worst);
    return 0;
}
