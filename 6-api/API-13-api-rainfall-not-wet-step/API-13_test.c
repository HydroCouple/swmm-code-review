/*
 * API-13: rainfall injected on a subcatchment with swmm_SUBCATCH_API_RAINFALL
 * does not make the runoff step a wet step.
 *
 * runoff_getTimeStep() uses WET_STEP only while a gage reports rain (or
 * there is snow, ponded runoff or a wet LID); otherwise it takes DRY_STEP,
 * limited by the time to the gage's next value. The rainfall a caller sets
 * on a subcatchment is added in getNetPrecip() but is not seen by that test,
 * so with a dry gage the first runoff step is a dry step computed with the
 * rate set at its start, and later changes inside it are ignored.
 *
 * Correct behaviour: the caller sets 2 in/hr on S1 before every swmm_step()
 * during the first 30 minutes and 0 afterwards (WET_STEP = ROUTING_STEP =
 * 1 min), so S1 receives 2 in/hr x 0.5 h = 1.000 in, and the runoff
 * continuity error is as small as when the same rain comes from a gage
 * (API-13_gage-rain.inp, about -0.1 %). The test reads Total Precipitation
 * and Continuity Error from the Runoff Quantity Continuity table of the
 * report and requires 1.000 +/- 0.01 in and an error within +/- 1 %. The
 * defect gives 2.000 in with a 1-h gage interval, and 1.000 in but a
 * continuity error of -17 % with a 30-min interval.
 *
 * 5.2.4 has no subcatchment rainfall property, so it is not affected.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
/* Total precipitation (in) and continuity error (%) from the Runoff Quantity
 * Continuity table of a report file. Returns 0 if both were found. */
static int readRunoffContinuity(const char *rpt, double *precip, double *err)
{
    char line[256];
    int inTable = 0, found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 1;
    while (fgets(line, sizeof line, f) && found < 2)
    {
        if (strstr(line, "Runoff Quantity Continuity")) inTable = 1;
        if (!inTable) continue;
        if (strstr(line, "Total Precipitation"))
        {
            char *p = strrchr(line, ' ');   /* last column: depth in inches */
            *precip = atof(p);
            found++;
        }
        else if (strstr(line, "Continuity Error (%)"))
        {
            char *p = strstr(line, ".....");
            while (p && *p == '.') p++;
            *err = p ? atof(p) : 0.0;
            found++;
        }
    }
    fclose(f);
    return found == 2 ? 0 : 1;
}

/* Runs inp; if useApi, sets 2 in/hr on S1 before every step in the first
 * 30 minutes and 0 afterwards. */
static int run(const char *inp, const char *rpt, int useApi, double *precip, double *err)
{
    double t = 0.0;
    int s1, e = swmm_open(inp, rpt, "API-13.out");
    if (!e) e = swmm_start(0);
    s1 = swmm_getIndex(swmm_SUBCATCH, "S1");
    while (!e)
    {
        if (useApi)
            e = swmm_setValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_API_RAINFALL, s1, -1, -1,
                                      (t < 0.5 / 24.0) ? 2.0 : 0.0);
        if (!e) e = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (e) return e;
    return readRunoffContinuity(rpt, precip, err) ? -1 : 0;
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    struct { const char *name, *inp, *rpt; int api; } cases[] = {
        { "rain from the gage (reference)", "API-13_gage-rain.inp",  "API-13_gage-rain.rpt",  0 },
        { "API, gage interval 30 min",      "API-13_gage30min.inp",  "API-13_gage30min.rpt",  1 },
        { "API, gage interval 1 h",         "API-13_gage1h.inp",     "API-13_gage1h.rpt",     1 },
    };
    int i, rc, nbad = 0;
    double precip, err;

    printf("2 in/hr for 30 min on S1, DRY_STEP 1 h, WET_STEP 1 min\n");
    printf("%-32s %18s %22s\n", "", "precipitation (in)", "runoff continuity (%)");
    for (i = 0; i < 3; i++)
    {
        precip = err = 0.0;
        rc = run(cases[i].inp, cases[i].rpt, cases[i].api, &precip, &err);
        if (rc) { printf("%-32s run error %d\n", cases[i].name, rc); nbad++; continue; }
        printf("%-32s %18.3f %22.2f\n", cases[i].name, precip, err);
        if (fabs(precip - 1.0) > 0.01 || fabs(err) > 1.0) nbad++;
    }
    if (nbad)
    {
        printf("FAIL: in %d of 3 runs the 1.000 in of rain was not applied as given "
               "(wrong depth or a runoff continuity error beyond 1 %%)\n", nbad);
        return 1;
    }
    printf("PASS: rain injected through the API gives 1.000 in and a small continuity "
           "error, like the same rain from a gage\n");
    return 0;
#else
    printf("PASS: not affected, 5.2.4 has no subcatchment rainfall property\n");
    return 0;
#endif
}
