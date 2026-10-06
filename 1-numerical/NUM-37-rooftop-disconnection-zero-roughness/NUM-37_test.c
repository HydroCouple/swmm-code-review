/*
 * NUM-37: a rooftop disconnection with Rough = 0 or Slope = 0 never sheds
 * any water; all rain stays on the roof.
 *
 * Each deck has a 1-acre rooftop disconnection (the whole subcatchment) with
 * 0.1 in of surface storage and gutters that carry at most 0.5 in/hr. 2.0 in
 * of rain falls at 1 in/hr. There is no infiltration and no evaporation.
 * The input reference says that with Rough or Slope = 0 "any ponded water
 * that exceeds the surface storage depth is assumed to completely overflow
 * the LID control within a single time step". So once 0.1 in is stored
 * (after 6 min) every further inch leaves the roof, the gutters take
 * 0.5 in/hr of it and the rest runs off the surface:
 *
 *     final storage        = 0.100 in
 *     surface runoff       = 1.9 - 0.95 = 0.950 in
 *     LID (gutter) drainage = 0.5 in/hr * 1.9 hr = 0.950 in
 *
 * With the bug nothing leaves: runoff 0, drainage 0, final storage 2.000 in.
 * The tolerance (0.02 in) is 50 times smaller than that error and covers the
 * one-minute step in which the roof storage fills.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* number in column col (0 = acre-feet, 1 = inches) of the report line
   containing label; 0 if the line is absent (LID Drainage is only printed
   when it is non-zero) */
static double rptValue(const char* rpt, const char* label, int col)
{
    char line[512];
    double v = 0.0;
    FILE* f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, label);
        double a, b;
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        if (sscanf(p, "%lf %lf", &a, &b) == 2) v = col ? b : a;
        break;
    }
    fclose(f);
    return v;
}

static int runDeck(const char* inp, const char* rpt, const char* out,
                   double* runoff, double* drain, double* store)
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
    *runoff = rptValue(rpt, "Surface Runoff", 1);
    *drain  = rptValue(rpt, "LID Drainage", 1);
    *store  = rptValue(rpt, "Final Storage", 1);
    return err;
}

int main(void)
{
    const char* names[2] = { "Rough = 0", "Slope = 0" };
    const char* inps[2]  = { "NUM-37_rough0.inp", "NUM-37_slope0.inp" };
    const char* rpts[2]  = { "NUM-37_rough0.rpt", "NUM-37_slope0.rpt" };
    const char* outs[2]  = { "NUM-37_rough0.out", "NUM-37_slope0.out" };
    double runoff[2], drain[2], store[2];
    int i, bad = -1;

    for (i = 0; i < 2; i++)
    {
        int err = runDeck(inps[i], rpts[i], outs[i], &runoff[i], &drain[i], &store[i]);
        if (err)
        {
            printf("FAIL: %s stopped with error %d\n", inps[i], err);
            return 1;
        }
    }

    printf("Roof          Surface runoff  LID drainage  Final storage   (in)\n");
    for (i = 0; i < 2; i++)
    {
        printf("%-12s  %14.3f  %12.3f  %13.3f\n", names[i], runoff[i], drain[i], store[i]);
        if ((fabs(store[i] - 0.100) > 0.02 || fabs(runoff[i] - 0.950) > 0.02 ||
             fabs(drain[i] - 0.950) > 0.02) && bad < 0) bad = i;
    }
    printf("expected      %14.3f  %12.3f  %13.3f\n", 0.950, 0.950, 0.100);

    if (bad >= 0)
    {
        printf("FAIL: with %s the roof kept %.3f in of 2.0 in of rain (storage depth "
               "0.1 in) and shed %.3f in (expected 1.900 in)\n",
               names[bad], store[bad], runoff[bad] + drain[bad]);
        return 1;
    }
    printf("PASS: with Rough or Slope = 0 water above the 0.1 in storage depth overflows; "
           "the gutters carry 0.5 in/hr of it\n");
    return 0;
}
