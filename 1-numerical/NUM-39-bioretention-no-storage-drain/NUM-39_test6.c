/*
 * NUM-39 for 6.0.0: a bio-retention cell or rain garden with no storage
 * layer but a DRAIN line reports underdrain flow that no layer loses.
 *
 * Each deck has a 1-acre LID unit (the whole subcatchment): 6 in of ponding
 * over 12 in of soil (Ksat 0.5 in/hr), no storage layer (a BC with a 0 in
 * STORAGE layer, and an RG, which has none) and an underdrain (C = 1,
 * n = 0.5). 2.0 in of rain falls in 2 hours. There is no evaporation and no
 * seepage, so the drain can only release water that has fallen or was
 * stored at the start and is no longer in the unit:
 *
 *     drain <= rain + initial storage - surface runoff - final storage
 *
 * and the runoff continuity error must be ~0. Without a storage layer the
 * drain rate is computed from the head in the soil but is subtracted from no
 * layer, so the drain runs for as long as the soil is above field capacity
 * (17 in from 2 in of rain). The tolerances (1 % on continuity, 0.05 in on
 * the drain volume) are far below the error (-537 %, about 17 in) and above
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

/* runs one deck; returns 1 if the drain released more than it had */
static int runCase(const char* name, const char* inp, const char* rpt,
                   const char* out, char* msg)
{
    double t = 0.0, init, rain, runoff, drain, store, cerr, avail;
    int err;

    SWMM_Engine e = swmm_engine_create();
    err = swmm_engine_open(e, inp, rpt, out, NULL);
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
        sprintf(msg, "%s: the run stopped with error %d", name, err);
        printf("%-22s run stopped with error %d\n", name, err);
        return 1;
    }

    init   = rptValue(rpt, "Initial LID Storage", 1);
    rain   = rptValue(rpt, "Total Precipitation", 1);
    runoff = rptValue(rpt, "Surface Runoff", 1);
    drain  = rptValue(rpt, "LID Drainage", 1);
    store  = rptValue(rpt, "Final Storage", 1);
    cerr   = rptValue(rpt, "Continuity Error (%)", 0);
    avail  = rain + init - runoff - store;

    printf("%-22s %7.3f %7.3f %7.3f %7.3f %8.3f %9.3f %10.3f\n",
           name, init, rain, runoff, store, drain, avail, cerr);

    if (rain < 1.9 || drain > avail + 0.05 || fabs(cerr) > 1.0)
    {
        sprintf(msg, "%s: the underdrain released %.3f in but only %.3f in "
                "left the unit's layers; continuity error %.3f %%",
                name, drain, avail, cerr);
        return 1;
    }
    return 0;
}

int main(void)
{
    char msg1[256] = "", msg2[256] = "";
    int bad1, bad2;

    printf("Runoff continuity (in)\n");
    printf("%-22s %7s %7s %7s %7s %8s %9s %10s\n", "LID", "init", "rain",
           "runoff", "final", "drain", "available", "error (%)");
    bad1 = runCase("BC, 0 in storage", "NUM-39_bc-no-storage.inp",
                   "NUM-39_bc6.rpt", "NUM-39_bc6.out", msg1);
    bad2 = runCase("RG with a drain", "NUM-39_rg-drain.inp",
                   "NUM-39_rg6.rpt", "NUM-39_rg6.out", msg2);

    if (bad1 || bad2)
    {
        printf("FAIL: %s\n", bad1 ? msg1 : msg2);
        return 1;
    }
    printf("PASS: without a storage layer the underdrain releases only water "
           "that leaves the soil; continuity closes\n");
    return 0;
}
