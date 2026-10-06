/*
 * CON-16 for 6.0.0: a subarea's runoff volume is booked as its END-of-step runoff rate
 * times the step, not as the water that left it during the step.
 *
 * The deck is a 1-acre impervious area with no depression storage, no
 * infiltration and no evaporation, and 2.0 in of rain (2 in/hr for 1 h). The
 * test runs it with WET_STEP = 1, 5, 15 and 30 min; it writes each variant as
 * CON-16_wetNN.inp next to itself. By continuity
 *
 *     surface runoff + final storage = rain = 2.000 in
 *
 * so the runoff continuity error must be ~0 for every step. With the bug the
 * runoff exceeds the rain: 2.011 in (-0.585 %) at 5 min and 2.062 in
 * (-3.12 %) at 15 min, the step the reference manual calls acceptable for
 * hourly rain. The limit of 0.1 % on the continuity error is 6 times smaller
 * than the 5-min error; at 1 min the error (-0.026 %) is within it.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

/* first number(s) after the dotted leader of the report line containing label;
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

/* copy the base deck, replacing its WET_STEP line */
static int writeDeck(const char* out, int wetMin)
{
    char line[512];
    FILE* fi = fopen("CON-16_impervious-roof.inp", "r");
    FILE* fo = fopen(out, "w");
    if (!fi || !fo) { if (fi) fclose(fi); if (fo) fclose(fo); return 0; }
    while (fgets(line, sizeof(line), fi))
    {
        if (strncmp(line, "WET_STEP", 8) == 0)
            fprintf(fo, "WET_STEP             00:%02d:00\n", wetMin);
        else fputs(line, fo);
    }
    fclose(fi);
    fclose(fo);
    return 1;
}

int main(void)
{
    const int wet[4] = {1, 5, 15, 30};
    int k, bad = 0;

    printf("WET_STEP   rain    runoff   final storage   continuity error\n");
    printf("  (min)    (in)    (in)     (in)            (%%)\n");
    for (k = 0; k < 4; k++)
    {
        char inp[64], rpt[64], out[64];
        double t = 0.0, rain = 0, runoff = 0, store = 0, runoffErr = 0;
        SWMM_Engine e;
        int err;

        sprintf(inp, "CON-16_wet%02d.inp", wet[k]);
        sprintf(rpt, "CON-16_wet%02d_6.rpt", wet[k]);
        sprintf(out, "CON-16_wet%02d_6.out", wet[k]);
        if (!writeDeck(inp, wet[k]))
        {
            printf("FAIL: could not write %s\n", inp);
            return 1;
        }
        e = swmm_engine_create();
        err = swmm_engine_open(e, inp, rpt, out, NULL);
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
        if (err ||
            !rptValue(rpt, "Total Precipitation", 1, &rain) ||
            !rptValue(rpt, "Surface Runoff", 1, &runoff) ||
            !rptValue(rpt, "Final Storage", 1, &store))
        {
            printf("FAIL: the run with WET_STEP %d min did not complete (error %d)\n",
                   wet[k], err);
            return 1;
        }
        printf("  %3d     %6.3f  %6.3f   %6.3f          %8.3f\n",
               wet[k], rain, runoff, store, runoffErr);
        if (fabs(runoffErr) > 0.1) bad++;
    }
    if (bad)
    {
        printf("FAIL: runoff + storage differs from the rain by more than 0.1 %% "
               "for %d of 4 wet steps\n", bad);
        return 1;
    }
    printf("PASS: runoff + storage = rain within 0.1 %% for every wet step\n");
    return 0;
}
