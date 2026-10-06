/*
 * IO-24 for 6.0.0: the rain barrel Covrd token on the [LID_CONTROLS] STORAGE line.
 *
 * IO-24_covrd.inp has three identical subcatchments, each with 100 rain barrels
 * of 50 ft2 that take no runoff (FromImp = 0) and have no drain. The only water
 * a barrel can receive is the 2.0 in of rain falling on its own top, and only
 * if it is uncovered. The three barrels differ only in the Covrd token:
 *   RB_YES  STORAGE 48 0.75 0 0 YES   covered    -> total inflow 0.00 in
 *   RB_NO   STORAGE 48 0.75 0 0 NO    uncovered  -> total inflow 2.00 in
 *   RB_DEF  STORAGE 48 0.75 0 0       no token   -> total inflow 2.00 in
 * The default is uncovered: that is what every release since 5.2.0 parses (and
 * what 5.1, which had no cover option, computed), so token-less decks keep their
 * results. The input reference's "YES (the default)" is the error (see README).
 *
 * The test reads each barrel's Total Inflow from the LID Performance Summary.
 * The expected values are 0 or 2.0 in; a limit of 0.1 in separates them.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* Total Inflow (3rd column) of the LID Performance Summary row for this LID */
static int lidInflow(const char* rpt, const char* lid, double* v)
{
    char line[512], a[64], b[64];
    int inTable = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "LID Performance Summary")) inTable = 1;
        if (!inTable) continue;
        if (sscanf(line, "%63s %63s %lf", a, b, v) == 3 &&
            (strcmp(a, lid) == 0 || strcmp(b, lid) == 0))
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main(void)
{
    const char* lid[3]  = {"RB_YES", "RB_NO", "RB_DEF"};
    const char* tok[3]  = {"YES", "NO", "(none)"};
    const double want[3] = {0.0, 2.0, 2.0};
    double elapsed = 0.0, got[3];
    int k, rc, bad = 0;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "IO-24_covrd.inp", "IO-24_covrd6.rpt", "IO-24_covrd6.out", NULL);
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

    printf("barrel   Covrd    barrel inflow (in)\n");
    printf("                  expected  reported\n");
    for (k = 0; k < 3; k++)
    {
        if (!lidInflow("IO-24_covrd6.rpt", lid[k], &got[k]))
        {
            printf("FAIL: no LID Performance Summary row for %s\n", lid[k]);
            return 1;
        }
        printf("%-8s %-8s %6.2f    %6.2f\n", lid[k], tok[k], want[k], got[k]);
        if (fabs(got[k] - want[k]) > 0.1) bad++;
    }
    if (bad)
    {
        printf("FAIL: %d of 3 barrels ignore their Covrd setting "
               "(RB_YES %.2f in, RB_NO %.2f in, no token %.2f in)\n",
               bad, got[0], got[1], got[2]);
        return 1;
    }
    printf("PASS: YES gives a covered barrel (0.00 in), NO and no token an uncovered one (2.00 in)\n");
    return 0;
}
