/*
 * API-07 for 6.0.0: subcatchment buildup with several land uses.
 *
 * 6.0.0 has no runtime getter for a subcatchment's pollutant buildup, so the
 * legacy defect has no counterpart. What can be checked is the buildup the
 * engine computes on the same deck: L1 2 ac x 50 lb/ac + L2 3 ac x 10 lb/ac +
 * L3 5 ac x 0 = 130 lb on 10 ac (13 lb/ac), read from the report's Initial
 * Buildup line (checked within 1%).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* "Initial Buildup ..........       130.000" from the Runoff Quality Continuity */
static double reportInitialBuildup(const char *rpt)
{
    char line[512];
    double x = -1.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof(line), f))
    {
        char *p = strstr(line, "Initial Buildup");
        if (p)
        {
            /* skip the dot leader to the first number */
            p += strlen("Initial Buildup");
            while (*p == ' ' || *p == '.') p++;
            sscanf(p, "%lf", &x);
            break;
        }
    }
    fclose(f);
    return x;
}

int main(void)
{
    double ib;
    int rc = swmm_engine_run("API-07_three-land-uses.inp", "API-07_6.rpt", "API-07_6.out", NULL);
    ib = reportInitialBuildup("API-07_6.rpt");
    printf("6.0.0 has no subcatchment buildup getter; report Initial Buildup = %.3f lb "
           "(%.4f lb/ac on 10 ac), expected 130 lb (13 lb/ac); run code %d\n", ib, ib / 10.0, rc);
    if (rc || fabs(ib - 130.0) > 1.3)
    {
        printf("FAIL: the buildup on S1 is not 130 lb\n");
        return 1;
    }
    printf("PASS: no buildup getter in 6.0.0, and the buildup it computes is 13 lb/ac\n");
    return 0;
}
