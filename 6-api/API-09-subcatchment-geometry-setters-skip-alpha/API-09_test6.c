/*
 * API-09 for 6.0.0: subcatchment area, width and slope set through the API
 * after swmm_engine_open() must reach the runoff calculation.
 *
 * 6.0.0 computes the overland-flow coefficient alpha from area, width, slope
 * and n when the runoff module is initialised (swmm_engine_initialize), so a
 * value set between open and initialize should be used. The check is the
 * same as for the legacy engine: the peak runoff of S1 after a setter call
 * must equal the peak of a run with the value in [SUBCATCHMENTS], to a
 * relative 1e-6 (the defect in 5.3.0 changes the peak by 50 to 80 %).
 *
 * Units per openswmm_subcatchments.h: area in project area units (acres),
 * width in project length units (ft), slope in percent; runoff in project
 * flow units (cfs).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"

enum { NONE, AREA, WIDTH, SLOPE };

static double peakRunoff(const char *inp, int prop, double value, int *rcSet)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, q = 0.0, qmax = 0.0;
    int s1, rc = swmm_engine_open(e, inp, "API-09_6.rpt", "API-09_6.out", NULL);

    *rcSet = 0;
    s1 = swmm_subcatch_index(e, "S1");
    if (!rc && prop == AREA)  *rcSet = swmm_subcatch_set_area(e, s1, value);
    if (!rc && prop == WIDTH) *rcSet = swmm_subcatch_set_width(e, s1, value);
    if (!rc && prop == SLOPE) *rcSet = swmm_subcatch_set_slope(e, s1, value);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_subcatch_get_runoff(e, s1, &q);
        if (q > qmax) qmax = q;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc ? -1.0 : qmax;
}

int main(void)
{
    struct { const char *name; int prop; double value; const char *inp; } cases[] = {
        { "width 500 -> 5000 ft", WIDTH, 5000.0, "API-09_width5000.inp" },
        { "slope 0.5 -> 5 %",     SLOPE, 5.0,    "API-09_slope5.inp"    },
        { "area 10 -> 20 ac",     AREA,  20.0,   "API-09_area20.inp"    },
    };
    int i, rc, unused, nbad = 0;
    double q0, qApi, qInp, rel, worst = 0.0;

    q0 = peakRunoff("API-09_base.inp", NONE, 0.0, &unused);
    printf("Peak runoff of S1 with API-09_base.inp unchanged: %.4f cfs\n\n", q0);
    printf("%-22s %4s %14s %14s %9s\n", "Set before initialize", "rc",
           "peak via API", "peak via .inp", "diff");
    for (i = 0; i < 3; i++)
    {
        qApi = peakRunoff("API-09_base.inp", cases[i].prop, cases[i].value, &rc);
        qInp = peakRunoff(cases[i].inp, NONE, 0.0, &unused);
        rel = (qInp > 0.0) ? fabs(qApi - qInp) / qInp : 1.0;
        printf("%-22s %4d %14.4f %14.4f %8.2f%%\n", cases[i].name, rc, qApi, qInp, 100.0 * rel);
        if (rc != 0 || qApi < 0.0 || qInp < 0.0 || rel > 1.0e-6) nbad++;
        if (rel > worst) worst = rel;
    }
    if (nbad)
    {
        printf("FAIL: %d of 3 subcatchment geometry setters did not change the runoff "
               "as the input file does (largest peak difference %.1f %%)\n", nbad, 100.0 * worst);
        return 1;
    }
    printf("PASS: area, width and slope set before initialize give the same runoff "
           "as the same values in the input file\n");
    return 0;
}
