/*
 * NUM-56 for 6.0.0 (a 5.2.4-only defect, so this passes): with
 * MODIFIED_HORTON, a maximum infiltration volume Fmax > 0 stops infiltration
 * after the first wet time step.
 *
 * NUM-56_fmax-not-binding.inp: 1 in of rain at 1 in/hr on two identical
 * pervious subcatchments, f0 = 3, fmin = 0.5 in/hr, decay 4/hr. S1 has
 * Fmax = 5 in, S2 none. Analytically the capacity is max(3 - 4 Fe, 0.5) with
 * Fe growing at f - fmin = 0.5 in/hr, so it is still 1.0 in/hr when the rain
 * stops after an hour: all 1.00 in infiltrates on both, with no runoff, and
 * an Fmax larger than the rainfall cannot change that (Vol I sec. 4.3.3:
 * Fe <- min(Fe + (f - fmin) dt, Fmax)).
 *
 * The test reads Total Infil and Total Runoff (in) of S1 and S2 from the
 * report's Subcatchment Runoff Summary. Tolerance 0.01 in (2 decimals in
 * the table); with the bug S1 infiltrates 0.08 in.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

#define TOL_IN 0.01

/* Total Infil and Total Runoff (in) of a row of Subcatchment Runoff Summary */
static int runoffRow(const char* rpt, const char* name, double* infil,
                     double* runoff)
{
    char line[512], id[64];
    int inTable = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        double v[7];
        if (!inTable)
        {
            if (strstr(line, "Subcatchment Runoff Summary")) inTable = 1;
            continue;
        }
        if (sscanf(line, "%63s %lf %lf %lf %lf %lf %lf %lf", id, &v[0], &v[1],
                   &v[2], &v[3], &v[4], &v[5], &v[6]) == 8 &&
            strcmp(id, name) == 0)
        {
            fclose(f);
            *infil = v[3];
            *runoff = v[6];
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main(void)
{
    const char* inp = "NUM-56_fmax-not-binding.inp";
    const char* rpt = "NUM-56_fmax-not-binding6.rpt";
    const char* out = "NUM-56_fmax-not-binding6.out";
    double t = 0.0, i1, r1, i2, r2;
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
    if (err)
    {
        printf("FAIL: run stopped with error %d\n", err);
        return 1;
    }
    if (!runoffRow(rpt, "S1", &i1, &r1) || !runoffRow(rpt, "S2", &i2, &r2))
    {
        printf("FAIL: could not read the Subcatchment Runoff Summary\n");
        return 1;
    }

    printf("Subcatchment  Fmax (in)  Infiltration (in)  Runoff (in)\n");
    printf("S1            5.0        %17.2f  %11.2f\n", i1, r1);
    printf("S2            none       %17.2f  %11.2f\n", i2, r2);
    printf("Expected for both        %17.2f  %11.2f\n", 1.0, 0.0);
    if (fabs(i1 - 1.0) > TOL_IN || fabs(i2 - 1.0) > TOL_IN)
    {
        printf("FAIL: an Fmax of 5 in that cannot bind cuts infiltration to "
               "%.2f in (expected 1.00 in, %.2f in without Fmax)\n", i1, i2);
        return 1;
    }
    printf("PASS: with Fmax = 5 in all 1.00 in of rain infiltrates, as "
           "without Fmax\n");
    return 0;
}
