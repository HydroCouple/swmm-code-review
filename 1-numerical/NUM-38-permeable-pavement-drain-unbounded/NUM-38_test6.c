/*
 * NUM-38 for 6.0.0: permeable pavement whose pavement and soil layers are full but
 * whose storage layer is not drains more water than the storage layer holds.
 *
 * The deck has a 1-acre permeable pavement (the whole subcatchment): 2 in of
 * pavement over 2 in of soil (Ksat 0.5 in/hr) over a 12 in gravel bed with an
 * impermeable floor and an underdrain (C = 50, n = 0.5). 9.0 in of rain falls
 * at 3 in/hr. There is no evaporation and no infiltration, so every inch that
 * does not run off the surface or stay in the unit must leave by the drain:
 *
 *     drain = rain + initial storage - surface runoff - final storage
 *
 * and the runoff continuity error must be ~0. Once the pavement and soil are
 * saturated, the drain rate (about 7 in/hr at the bottom of the gravel bed)
 * is no longer limited by the water in the gravel bed, which only receives
 * 0.5 in/hr from the soil; the solver clamps the storage depth at 0 and the
 * extra drain flow is created. The tolerances (1 % on continuity, 0.05 in on
 * the drain volume) are far below the error (-85 %, about 8 in) and above
 * the 3-decimal rounding of the report.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* number in column col (0 = acre-feet, 1 = inches) of the report line
   containing label; 0 if the line is absent */
static double rptValue(const char* rpt, const char* label, int col)
{
    char line[512];
    double v = 0.0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0.0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, label);
        double a, b;
        int n;
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        n = sscanf(p, "%lf %lf", &a, &b);
        if (n > col) v = col ? b : a;
        break;
    }
    fclose(f);
    return v;
}

int main(void)
{
    const char* rpt = "NUM-38_6.rpt";
    double t = 0.0, init, rain, runoff, drain, store, cerr, avail;
    int err;

    SWMM_Engine e = swmm_engine_create();
    err = swmm_engine_open(e, "NUM-38_pp-slow-soil.inp", rpt, "NUM-38_6.out", NULL);
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
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    init   = rptValue(rpt, "Initial LID Storage", 1);
    rain   = rptValue(rpt, "Total Precipitation", 1);
    runoff = rptValue(rpt, "Surface Runoff", 1);
    drain  = rptValue(rpt, "LID Drainage", 1);
    store  = rptValue(rpt, "Final Storage", 1);
    cerr   = rptValue(rpt, "Continuity Error (%)", 0);
    avail  = rain + init - runoff - store;

    printf("Runoff continuity (in)\n");
    printf("  initial LID storage   %7.3f\n", init);
    printf("  precipitation         %7.3f\n", rain);
    printf("  surface runoff        %7.3f\n", runoff);
    printf("  final storage         %7.3f\n", store);
    printf("  LID drainage          %7.3f   (water available for it: %.3f)\n", drain, avail);
    printf("  continuity error (%%)  %7.3f\n", cerr);

    if (rain < 8.9 || drain > avail + 0.05 || fabs(cerr) > 1.0)
    {
        printf("FAIL: the underdrain released %.3f in but only %.3f in reached it; "
               "continuity error %.3f %%\n", drain, avail, cerr);
        return 1;
    }
    printf("PASS: the underdrain releases only the water that reaches it "
           "(%.3f in), continuity error %.3f %%\n", drain, cerr);
    return 0;
}
