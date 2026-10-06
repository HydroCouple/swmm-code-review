/*
 * CRASH-03 for 6.0.0: a curve or time series without data points.
 *
 * 6.0.0 does not crash on these decks, but it accepts them: the storage node
 * SU1 runs with an empty storage curve and J1 with an inflow series that has
 * no values, and nothing is reported. The legacy check this mirrors
 * (table_validate) rejects such tables once its crash is fixed.
 *
 * Correct behaviour: opening the model fails with ERROR 171 for the curve and
 * ERROR 173 for the time series, as in 5.3.0, and nothing crashes.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

static int reportHas(const char *rpt, const char *text)
{
    char line[256];
    int found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (!found && fgets(line, sizeof line, f))
        if (strstr(line, text))
        {
            found = 1;
            printf("    report: %s", line + strspn(line, " "));
        }
    fclose(f);
    return found;
}

static int check(const char *inp, const char *rpt, const char *out, int expect)
{
    char text[32];
    double t = 0.0;
    int rc, rcRun = 0, found;
    SWMM_Engine e = swmm_engine_create();
    printf("%s:\n", inp);
    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    printf("    swmm_engine_open returned %d\n", rc);
    if (!rc)
    {
        /* accepted: run it to show that it runs without a message */
        rcRun = swmm_engine_initialize(e);
        if (!rcRun) rcRun = swmm_engine_start(e, 1);
        while (!rcRun) { rcRun = swmm_engine_step(e, &t); if (t <= 0) break; }
        if (!rcRun) rcRun = swmm_engine_end(e);
        if (!rcRun) rcRun = swmm_engine_report(e);
        printf("    the run returned %d\n", rcRun);
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    sprintf(text, "ERROR %d", expect);
    found = reportHas(rpt, text);
    return rc != 0 && found;
}

int main(void)
{
    int ok1 = check("CRASH-03_empty-curve.inp", "CRASH-03_curve6.rpt", "CRASH-03_curve6.out", 171);
    int ok2 = check("CRASH-03_empty-timeseries.inp", "CRASH-03_ts6.rpt", "CRASH-03_ts6.out", 173);
    if (!ok1 || !ok2)
    {
        printf("FAIL: a table without data is not rejected (curve %s, time series %s)\n",
               ok1 ? "ok" : "accepted", ok2 ? "ok" : "accepted");
        return 1;
    }
    printf("PASS: the curve and the time series without data are rejected "
           "with ERROR 171 and 173\n");
    return 0;
}
