/*
 * NUM-33: Horton and Modified Horton ignore the maximum infiltration volume
 * (Fmax) when the curve is a constant rate (f0 == fmin or decay == 0).
 *
 * Both decks have six pervious subcatchments with Fmax = 0.5 in:
 *   H1, M1  decaying curve f0 3, fmin 0.5 in/hr, decay 4/hr (reference)
 *   H2, M2  constant rate f0 = fmin = 3 in/hr
 *   H3, M3  constant rate through decay = 0
 * (H = HORTON, M = MODIFIED_HORTON). Rain is 1 in/hr for one hour, all of
 * which a 3 in/hr soil would take, so the cap decides the result.
 *
 *   NUM-33_one-storm.inp   one storm: infiltration must not exceed Fmax.
 *                          With the bug H2, H3, M2, M3 take the whole 1.00 in.
 *                          Tolerance 0.01 in = the report's rounding; the bug
 *                          is 50 times larger.
 *   NUM-33_two-storms.inp  the storm again 10 days later; drying time 7 days,
 *                          so the soil has recovered (to 2 % of Fmax after
 *                          7 days) and must take about Fmax again: 1.00 in in
 *                          total. The bug gives 2.00 in; a cap without
 *                          recovery would give 0.50 in. Accepted: 0.95-1.01.
 *
 * Only the constant-rate rows are checked. The decaying-curve rows are printed
 * for reference (M1 on 5.2.4 shows a different bug, NUM-56).
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* Total Infil (in) of subcatchment name from the Subcatchment Runoff Summary */
static int rptInfil(const char* rpt, const char* name, double* infil)
{
    char line[512], tok[64];
    int inTable = 0;
    double precip, runon, evap;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "Subcatchment Runoff Summary")) inTable = 1;
        if (!inTable) continue;
        if (sscanf(line, "%63s %lf %lf %lf %lf", tok, &precip, &runon, &evap, infil) == 5
            && strcmp(tok, name) == 0)
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int runDeck(const char* inp, const char* rpt, const char* out)
{
    double elapsed = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    return err;
}

int main(void)
{
    const char* names[6] = {"H1", "H2", "H3", "M1", "M2", "M3"};
    const char* kind[6] = {"HORTON decaying (ref.)", "HORTON f0 = fmin", "HORTON decay = 0",
                           "MOD_HORTON decaying (ref.)", "MOD_HORTON f0 = fmin",
                           "MOD_HORTON decay = 0"};
    const double Fmax = 0.5;
    double one[6], two[6];
    int i, nbad1 = 0, nbad2 = 0, err;
    double worst1 = 0.0, worst2 = Fmax * 2;

    err = runDeck("NUM-33_one-storm.inp", "NUM-33_one.rpt", "NUM-33_one.out");
    if (!err) err = runDeck("NUM-33_two-storms.inp", "NUM-33_two.rpt", "NUM-33_two.out");
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    for (i = 0; i < 6; i++)
    {
        if (!rptInfil("NUM-33_one.rpt", names[i], &one[i]) ||
            !rptInfil("NUM-33_two.rpt", names[i], &two[i]))
        {
            printf("FAIL: could not read the infiltration of %s from the report\n", names[i]);
            return 1;
        }
    }

    printf("Fmax = %.2f in, 1 in of rain per storm\n\n", Fmax);
    printf("Sub  Soil                          Infil (in)   Infil (in)\n");
    printf("                                    one storm   two storms\n");
    for (i = 0; i < 6; i++)
    {
        int ref = (i == 0 || i == 3);
        int bad1 = !ref && one[i] > Fmax + 0.01;
        int bad2 = !ref && (two[i] > 2 * Fmax + 0.01 || two[i] < 2 * Fmax - 0.05);
        printf("%-4s %-28s %9.2f %12.2f%s\n", names[i], kind[i], one[i], two[i],
               bad1 ? "   <-- more than Fmax in one storm" :
               bad2 ? "   <-- not Fmax per storm" : "");
        if (bad1) { nbad1++; if (one[i] > worst1) worst1 = one[i]; }
        if (bad2) { nbad2++; worst2 = two[i]; }
    }
    printf("\n");

    if (nbad1 || nbad2)
    {
        printf("FAIL: %d of 4 constant-rate soils infiltrate more than Fmax = %.2f in "
               "in one storm (up to %.2f in), %d of 4 do not take Fmax per storm over "
               "two storms (%.2f in instead of %.2f in)\n",
               nbad1, Fmax, worst1, nbad2, worst2, 2 * Fmax);
        return 1;
    }
    printf("PASS: every constant-rate soil stops at Fmax = %.2f in per storm and "
           "recovers between storms\n", Fmax);
    return 0;
}
