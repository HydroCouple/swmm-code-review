/*
 * NUM-31 for 6.0.0: the volume an outfall routes onto a subcatchment is turned into a
 * rate with the PREVIOUS runoff step and applied over the NEXT one.
 *
 * In both decks roof S1 drains through storage ST1 to outfall O1, whose flow
 * is routed onto pervious subcatchment S2 (infiltration 10 in/hr, so S2 takes
 * in all of it). S1's runoff stops when the rain stops at 1:00 and the runoff
 * step changes from WET_STEP (5 min) to DRY_STEP (1 h or 4 h) while O1 is
 * still discharging. There is no evaporation and nothing ponds, so by
 * continuity
 *
 *     rain + outfall runon = infiltration + runoff + final storage
 *
 * and the runoff continuity error must be ~0. With the bug S2 receives
 * vRouted * newStep / oldStep while vRouted is booked as outfall runon:
 * -1.98 % (1 h dry step) and -8.46 % (4 h dry step). The limit of 0.2 % is
 * 10 times smaller than the smaller of the two; S1 (n = 0, no depression
 * storage) and S2 (no ponding) have no other source of continuity error.
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

/* water S2 received from O1 (in): S2 has no evaporation, no runoff and no
   ponding, so it is S2's Total Infil minus its Total Precip in the
   Subcatchment Runoff Summary (6.0.0 prints its Total Runon column as 0) */
static int rptS2Received(const char* rpt, double* v)
{
    char line[512], name[64];
    int inTable = 0;
    double precip, runon, evap, infil;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "Subcatchment Runoff Summary")) inTable = 1;
        if (inTable && sscanf(line, "%63s %lf %lf %lf %lf", name, &precip, &runon,
                              &evap, &infil) == 5 && strcmp(name, "S2") == 0)
        {
            fclose(f);
            *v = infil - precip;
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int runDeck(const char* inp, const char* rpt, const char* out, const char* label)
{
    double t = 0.0, runon = 0, s2runon = 0, runoffErr = 0;
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
    if (!err) swmm_get_runoff_continuity_error(e, &runoffErr);   /* a fraction */
    runoffErr *= 100.0;
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("%-10s run stopped with error %d\n", label, err);
        return -1;
    }
    if (!rptValue(rpt, "Outfall Runon", 1, &runon) || !rptS2Received(rpt, &s2runon))
    {
        printf("%-10s could not read %s\n", label, rpt);
        return -1;
    }
    /* the continuity table's inches are over both subcatchments (2 ac);
       S2 is 1 ac, so the booked outfall runon on S2 is twice that depth */
    printf("%-10s %14.2f %15.2f %16.3f\n", label, 2.0 * runon, s2runon, runoffErr);
    return fabs(runoffErr) > 0.2;
}

int main(void)
{
    int bad = 0, r;
    printf("DRY_STEP   outfall runon  received by S2  runoff continuity\n");
    printf("           booked (in)    (in)            error (%%)\n");
    r = runDeck("NUM-31_dry-step-1h.inp", "NUM-31_1h_6.rpt", "NUM-31_1h_6.out", "1 h");
    if (r < 0) { printf("FAIL: the 1 h run did not complete\n"); return 1; }
    bad += r;
    r = runDeck("NUM-31_dry-step-4h.inp", "NUM-31_4h_6.rpt", "NUM-31_4h_6.out", "4 h");
    if (r < 0) { printf("FAIL: the 4 h run did not complete\n"); return 1; }
    bad += r;
    if (bad)
    {
        printf("FAIL: S2 does not receive the volume booked as outfall runon; "
               "runoff continuity error above 0.2 %% in %d of 2 runs\n", bad);
        return 1;
    }
    printf("PASS: S2 receives the routed outfall volume; runoff continuity error "
           "within 0.2 %% in both runs\n");
    return 0;
}
