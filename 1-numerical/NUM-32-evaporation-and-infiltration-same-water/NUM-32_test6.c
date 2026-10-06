/*
 * NUM-32 for 6.0.0: when ponded water on a pervious area is used up within one
 * step, both the full evaporation and the full infiltration are charged to it.
 *
 * The deck puts 0.5 in of rain (1 in/hr for 30 min) on a pervious area that
 * infiltrates 0.5 in/hr, leaving about 0.24 in ponded below its 0.3 in of
 * depression storage, so there is never any runoff. Then a 6-hour dry step
 * uses that water up with evaporation of 0.5 in/day. By continuity
 *
 *     evaporation + infiltration + runoff + final storage = rain = 0.500 in
 *
 * With the bug infiltration takes all the ponded water and evaporation is
 * charged on top of it: E x 6 h = 0.125 in of losses that never existed, a
 * -25 % runoff continuity error. The tolerance of 0.01 in is 12 times smaller
 * than that and well above the 3-decimal rounding of the report. Nothing in
 * the deck runs off, so no other continuity error can enter.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

/* first number after the dotted leader of the report line containing label;
   col = 0 for the first column (acre-feet), 1 for the second (inches) */
static int rptValue(const char* rpt, const char* label, int col, double* v)
{
    char line[512];
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, label);
        double a, b;
        int n;
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        n = sscanf(p, "%lf %lf", &a, &b);
        fclose(f);
        if (n < col + 1) return 0;
        *v = col ? b : a;
        return 1;
    }
    fclose(f);
    return 0;
}

int main(void)
{
    double t = 0.0, rain = 0, runoff = 0, store = 0, cerr = 0, runoffErr = 0, evap = 0, infil = 0;
    SWMM_Engine e = swmm_engine_create();
    int err = swmm_engine_open(e, "NUM-32_pond-dries-in-one-step.inp",
                               "NUM-32_6.rpt", "NUM-32_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) swmm_get_runoff_continuity_error(e, &runoffErr);   /* a fraction */
    runoffErr *= 100.0;
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    if (!rptValue("NUM-32_6.rpt", "Total Precipitation", 1, &rain) ||
        !rptValue("NUM-32_6.rpt", "Evaporation Loss", 1, &evap) ||
        !rptValue("NUM-32_6.rpt", "Infiltration Loss", 1, &infil) ||
        !rptValue("NUM-32_6.rpt", "Surface Runoff", 1, &runoff) ||
        !rptValue("NUM-32_6.rpt", "Final Storage", 1, &store) ||
        !rptValue("NUM-32_6.rpt", "Continuity Error (%)", 0, &cerr))
    {
        printf("FAIL: could not read the runoff continuity table in NUM-32_6.rpt\n");
        return 1;
    }

    printf("Total precipitation (in)  %6.3f\n", rain);
    printf("Evaporation loss (in)     %6.3f\n", evap);
    printf("Infiltration loss (in)    %6.3f\n", infil);
    printf("Surface runoff (in)       %6.3f\n", runoff);
    printf("Final storage (in)        %6.3f\n", store);
    printf("Losses + runoff + storage %6.3f   (must equal the rain)\n",
           evap + infil + runoff + store);
    printf("Continuity error (%%)      %6.3f   (engine: %.3f)\n", cerr, runoffErr);

    if (fabs(evap + infil + runoff + store - rain) > 0.01)
    {
        printf("FAIL: evaporation %.3f in + infiltration %.3f in + runoff %.3f in + "
               "storage %.3f in = %.3f in from %.3f in of rain (continuity error %.3f %%)\n",
               evap, infil, runoff, store, evap + infil + runoff + store, rain, runoffErr);
        return 1;
    }
    printf("PASS: evaporation %.3f in + infiltration %.3f in = rain %.3f in, "
           "continuity error %.3f %%\n", evap, infil, rain, runoffErr);
    return 0;
}
