/*
 * NUM-30 for 6.0.0: pervious runoff routed to the impervious area is lost when all
 * of the impervious area has no depression storage (PctZero = 100).
 *
 * The deck has a 1-acre, 50 % impervious subcatchment with RouteTo
 * IMPERVIOUS 100 % and PctZero 100, no infiltration and no evaporation.
 * 1.0 in of rain falls. By continuity
 *
 *     surface runoff + final surface storage = rain = 1.000 in
 *
 * With the bug the routed pervious runoff leaves the pervious sub-area but is
 * never added to an impervious sub-area: runoff is 0.504 in and storage
 * 0.027 in, so 0.469 in of rain is missing (runoff continuity error +47 %).
 * The tolerance of 0.02 in (2 % of the rain) is 20 times smaller than the
 * missing volume; it is larger than the ~0.5 % continuity error that the
 * end-of-step runoff rate (CON-16) gives in this deck with any PctZero < 100.
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
    double t = 0.0, rain = 0, runoff = 0, store = 0, cerr = 0, runoffErr = 0;
    SWMM_Engine e = swmm_engine_create();
    int err = swmm_engine_open(e, "NUM-30_pctzero100-route-to-imperv.inp",
                               "NUM-30_6.rpt", "NUM-30_6.out", NULL);
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
    if (!rptValue("NUM-30_6.rpt", "Total Precipitation", 1, &rain) ||
        !rptValue("NUM-30_6.rpt", "Surface Runoff", 1, &runoff) ||
        !rptValue("NUM-30_6.rpt", "Final Storage", 1, &store) ||
        !rptValue("NUM-30_6.rpt", "Continuity Error (%)", 0, &cerr))
    {
        printf("FAIL: could not read the runoff continuity table in NUM-30_6.rpt\n");
        return 1;
    }

    printf("Total precipitation (in)  %6.3f\n", rain);
    printf("Surface runoff (in)       %6.3f\n", runoff);
    printf("Final storage (in)        %6.3f\n", store);
    printf("Runoff + storage (in)     %6.3f   (must equal the rain)\n", runoff + store);
    printf("Continuity error (%%)      %6.3f   (engine: %.3f)\n", cerr, runoffErr);

    if (fabs(runoff + store - rain) > 0.02)
    {
        printf("FAIL: runoff %.3f in + storage %.3f in = %.3f in, but %.3f in of rain fell "
               "and there are no losses (continuity error %.3f %%)\n",
               runoff, store, runoff + store, rain, runoffErr);
        return 1;
    }
    printf("PASS: runoff %.3f in + storage %.3f in = rain %.3f in, continuity error %.3f %%\n",
           runoff, store, rain, runoffErr);
    return 0;
}
