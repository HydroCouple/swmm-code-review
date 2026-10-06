/*
 * IO-60 for 6.0.0: the LID Performance Summary reports a 100 % continuity error for a
 * LID unit that received no water.
 *
 * IO-60_idle-barrel.inp has two identical subcatchments, each with 10 empty
 * rain barrels that take half of the impervious runoff. S1's gage records no
 * rain, so its barrels receive nothing and release nothing: inflow, outflow
 * and storage are all 0, and the balance error is 0. S2 gets 1.0 in of rain
 * and its barrels a correct balance (about 0 %).
 *
 * The test reads the Continuity Error column (the 8th number of each row of the
 * LID Performance Summary) and requires it to be within 1 % of 0 for both
 * units; the bug prints 100.00 for S1. If a report has no continuity column,
 * as in 6.0.0, there is no error to misreport and the row passes.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* numbers of the LID Performance Summary row for subcatchment sub and LID lid;
   returns how many were read (up to 8) */
static int lidRow(const char* rpt, const char* sub, const char* lid, double x[8])
{
    char line[512], a[64], b[64];
    int inTable = 0, n = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "LID Performance Summary")) inTable = 1;
        if (!inTable) continue;
        if (sscanf(line, "%63s %63s", a, b) == 2 &&
            ((strcmp(a, sub) == 0 && strcmp(b, lid) == 0) ||
             (strcmp(a, lid) == 0 && strcmp(b, sub) == 0)))
        {
            n = sscanf(line, "%*s %*s %lf %lf %lf %lf %lf %lf %lf %lf",
                       &x[0], &x[1], &x[2], &x[3], &x[4], &x[5], &x[6], &x[7]);
            break;
        }
    }
    fclose(f);
    return n < 0 ? 0 : n;
}

int main(void)
{
    const char* sub[2] = {"S1", "S2"};
    const char* desc[2] = {"no rain", "1.0 in rain"};
    double elapsed = 0.0, x[8];
    int k, n, rc, bad = 0;
    char badMsg[256] = "";

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "IO-60_idle-barrel.inp", "IO-60_idle-barrel6.rpt",
                          "IO-60_idle-barrel6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run did not complete (error %d)\n", rc);
        return 1;
    }

    printf("unit     gage          inflow   final    continuity\n");
    printf("                        (in)   storage   error (%%)\n");
    for (k = 0; k < 2; k++)
    {
        n = lidRow("IO-60_idle-barrel6.rpt", sub[k], "RB1", x);
        if (n < 5)
        {
            printf("FAIL: no LID Performance Summary row for %s RB1\n", sub[k]);
            return 1;
        }
        if (n >= 8)
        {
            printf("%-3s RB1  %-12s %7.2f  %7.2f  %10.2f\n", sub[k], desc[k], x[0], x[6], x[7]);
            if (fabs(x[7]) > 1.0)
            {
                bad++;
                sprintf(badMsg + strlen(badMsg), " %s RB1 %.2f %% with %.2f in inflow",
                        sub[k], x[7], x[0]);
            }
        }
        else printf("%-3s RB1  %-12s %7.2f      (no continuity column)\n", sub[k], desc[k], x[0]);
    }
    if (bad)
    {
        printf("FAIL: the LID Performance Summary reports a continuity error for "
               "%d unit(s) that balance exactly:%s\n", bad, badMsg);
        return 1;
    }
    printf("PASS: no LID unit is reported with a continuity error, including the one that received no water\n");
    return 0;
}
