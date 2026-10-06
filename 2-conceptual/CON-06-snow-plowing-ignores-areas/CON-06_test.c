/*
 * CON-06: snow plowed to another subcatchment is scaled by area fractions
 * only, so its volume changes by the ratio of the two subcatchments' areas.
 *
 * snow_plowSnow() (snow.c) adds sfrac[4] * exc * fArea_j / fArea_m to the
 * depth of the receiving subcatchment's pervious pack. fArea are fractions of
 * each subcatchment's own area, so the volume is conserved only when the two
 * subcatchments have the same (non-LID) area; otherwise it is multiplied by
 * A_m / A_j.
 *
 * In both decks 1 in of snow falls on S1 (10 ac, all plowable) and S2, and
 * everything S1 collects is plowed onto S2. There is no infiltration or
 * evaporation and all snow melts within the run, so precipitation must come
 * out as runoff. Correct behaviour: the runoff continuity error stays within
 * 1%. With S2 = 100 ac the bug creates snow (about -80%); with S2 = 1 ac it
 * destroys snow (about +80%).
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* first number after the dots of a report line starting with `label` */
static double rpt_value(const char *rpt, const char *label)
{
    char line[256];
    double v = -1.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return v;
    while (fgets(line, sizeof line, f))
    {
        char *p = strstr(line, label);
        if (!p) continue;
        p += strlen(label);
        while (*p == ' ' || *p == '.') p++;
        if (sscanf(p, "%lf", &v) != 1) v = -1.0;
        break;
    }
    fclose(f);
    return v;
}

static int run(const char *inp, const char *rpt, const char *out, float *err)
{
    double t = 0.0;
    float flowErr, qualErr;
    int rc = swmm_open(inp, rpt, out);
    if (!rc) rc = swmm_start(1);
    while (!rc)
    {
        rc = swmm_step(&t);
        if (t <= 0.0) break;
    }
    if (!rc) rc = swmm_end();
    if (!rc) rc = swmm_getMassBalErr(err, &flowErr, &qualErr);
    if (!rc) rc = swmm_report();
    swmm_close();
    return rc;
}

int main(void)
{
    const char *name[2] = {"S2 = 100 ac", "S2 = 1 ac"};
    const char *inp[2] = {"CON-06_to-larger.inp", "CON-06_to-smaller.inp"};
    const char *rpt[2] = {"CON-06_to-larger.rpt", "CON-06_to-smaller.rpt"};
    const char *out[2] = {"CON-06_to-larger.out", "CON-06_to-smaller.out"};
    float err[2] = {0.0f, 0.0f};
    int i, rc, ok = 1;

    printf("\nSnow plowed from S1 (10 ac) to S2      volumes in acre-feet\n");
    printf("Case          Precipitation  Runoff   Final snow  Final storage  Continuity error\n");
    for (i = 0; i < 2; i++)
    {
        rc = run(inp[i], rpt[i], out[i], &err[i]);
        if (rc)
        {
            printf("FAIL: %s stopped with error %d\n", inp[i], rc);
            return 1;
        }
        printf("%-12s  %10.3f    %7.3f   %8.3f     %8.3f        %8.3f %%\n",
               name[i], rpt_value(rpt[i], "Total Precipitation"),
               rpt_value(rpt[i], "Surface Runoff"),
               rpt_value(rpt[i], "Final Snow Cover"),
               rpt_value(rpt[i], "Final Storage"), err[i]);
        /* the fixed code gives about -0.1%; the bug about +-80% */
        if (err[i] > 1.0f || err[i] < -1.0f) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: plowing snow to another subcatchment changes its volume: "
               "runoff continuity errors %.3f%% and %.3f%%\n", err[0], err[1]);
        return 1;
    }
    printf("PASS: plowed snow keeps its volume (runoff continuity errors "
           "%.3f%% and %.3f%%)\n", err[0], err[1]);
    return 0;
}
