/*
 * NUM-29: a subarea with Manning's n = 0 books the depression-storage fill
 * volume a second time as runoff.
 *
 * The deck has one 1-acre pervious subcatchment with N-Perv = 0 (no routing:
 * the excess over depression storage leaves within the step) and 0.5 in of
 * depression storage. 0.9 in of rain falls at 0.9 in/hr, so storage fills
 * after 33.3 min, 3.3 min into a 5-min runoff step. There is no infiltration
 * and no evaporation, so by continuity:
 *
 *     surface runoff = rain - depression storage = 0.9 - 0.5 = 0.400 in
 *
 * and the runoff continuity error must be ~0. With the bug the runoff of the
 * step in which storage fills is computed as excess/(time after filling) but
 * booked over the whole step, giving 0.450 in and a -5.6 % continuity error.
 * The tolerances (0.01 in on runoff, 0.1 % on continuity) are 5 and 50 times
 * smaller than the error and well above the 3-decimal rounding of the report.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

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
    double elapsed = 0.0, rain = 0, runoff = 0, store = 0, cerr = 0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err;

    err = swmm_open("NUM-29_n0-storage-fills-mid-step.inp", "NUM-29.rpt", "NUM-29.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    if (!rptValue("NUM-29.rpt", "Total Precipitation", 1, &rain) ||
        !rptValue("NUM-29.rpt", "Surface Runoff", 1, &runoff) ||
        !rptValue("NUM-29.rpt", "Final Storage", 1, &store) ||
        !rptValue("NUM-29.rpt", "Continuity Error (%)", 0, &cerr))
    {
        printf("FAIL: could not read the runoff continuity table in NUM-29.rpt\n");
        return 1;
    }

    printf("                         report   expected\n");
    printf("Total precipitation (in)  %6.3f     0.900\n", rain);
    printf("Surface runoff (in)       %6.3f     0.400\n", runoff);
    printf("Final storage (in)        %6.3f     0.500\n", store);
    printf("Continuity error (%%)      %6.3f     0.000  (swmm_getMassBalErr: %.3f)\n",
           cerr, runoffErr);

    if (fabs(runoff - 0.400) > 0.01 || fabs(runoffErr) > 0.1)
    {
        printf("FAIL: N = 0 subarea produced %.3f in of runoff from 0.900 in of rain "
               "and 0.500 in of storage (expected 0.400 in); continuity error %.3f %%\n",
               runoff, runoffErr);
        return 1;
    }
    printf("PASS: runoff = rain - depression storage = %.3f in, continuity error %.3f %%\n",
           runoff, runoffErr);
    return 0;
}
