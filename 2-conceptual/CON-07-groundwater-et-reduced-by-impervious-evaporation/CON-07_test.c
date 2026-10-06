/*
 * CON-07: water evaporating from the IMPERVIOUS surface is subtracted from
 * the evaporation available to the groundwater zones.
 *
 * Vol I eq. 5-11/5-12: upper-zone ET is min(e_max - e_s, UEF e_max), where
 * e_s is the evaporation of rainfall and ponded water on the PERVIOUS
 * surface; lower-zone ET gets what is left. Water on the impervious surface
 * plays no part.
 *
 * CON-07_impervious-ponding.inp has two subcatchments that differ only in the
 * depression storage of their impervious half: S1 keeps 0.5 in ponded there,
 * S2 none. Their pervious halves, aquifers and weather are identical, so the
 * total groundwater ET of S1 and S2 (column "Total Evap" of the report's
 * Groundwater Summary, inches) must be the same.
 *
 * Tolerance 0.01 in: the column has 2 decimals. With the bug S1 loses
 * 0.61 in and S2 0.81 in.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define TOL_IN 0.01

/* "Total Evap" (2nd number) of a subcatchment's row in Groundwater Summary */
static int gwEvap(const char* rpt, const char* name, double* v)
{
    char line[512], id[64];
    int inTable = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        double infil, evap;
        if (!inTable)
        {
            if (strstr(line, "Groundwater Summary")) inTable = 1;
            continue;
        }
        if (sscanf(line, "%63s %lf %lf", id, &infil, &evap) == 3 &&
            strcmp(id, name) == 0)
        {
            fclose(f);
            *v = evap;
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main(void)
{
    const char* inp = "CON-07_impervious-ponding.inp";
    const char* rpt = "CON-07_impervious-ponding.rpt";
    const char* out = "CON-07_impervious-ponding.out";
    double elapsed = 0.0, e1, e2;
    int err;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: run stopped with error %d\n", err);
        return 1;
    }
    if (!gwEvap(rpt, "S1", &e1) || !gwEvap(rpt, "S2", &e2))
    {
        printf("FAIL: could not read the Groundwater Summary table\n");
        return 1;
    }

    printf("Subcatchment  Impervious ponding  Groundwater ET (in)\n");
    printf("S1            0.5 in              %.2f\n", e1);
    printf("S2            none                %.2f\n", e2);
    if (fabs(e1 - e2) > TOL_IN)
    {
        printf("FAIL: ponded water on the impervious area changes groundwater ET "
               "(%.2f in vs %.2f in with identical pervious areas)\n", e1, e2);
        return 1;
    }
    printf("PASS: groundwater ET does not depend on impervious ponding "
           "(%.2f in vs %.2f in)\n", e1, e2);
    return 0;
}
