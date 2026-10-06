/*
 * API-13 for 6.0.0: rainfall forced on a subcatchment through the API must
 * be applied as given, also when the rain gage is dry.
 *
 * 6.0.0 has swmm_forcing_subcatch_rainfall() (openswmm_forcing.h), which
 * overrides a subcatchment's gage rainfall during a run. The runoff step is
 * WET_STEP only while a gage reports rain, or there is snow, ponded runoff or
 * a wet LID (SWMMEngine::computeRunoffTimestep), as in the legacy engine.
 *
 * Same check as the legacy test: 2 in/hr forced on S1 (OVERRIDE, PERSIST)
 * before every step during the first 30 minutes and 0 afterwards must give
 * 1.000 +/- 0.01 in of precipitation and a runoff continuity error within
 * +/- 1 %, read from the Runoff Quantity Continuity table of the report.
 * Rainfall rates are in user units (in/hr); elapsed time is in days.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"
#include "openswmm/engine/openswmm_forcing.h"

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

/* Runs inp; if useApi, forces 2 in/hr on S1 before every step in the first
 * 30 minutes and 0 afterwards. */
static int run(const char *inp, const char *rpt, int useApi, double *precip, double *err)
{
    SWMM_Engine eng = swmm_engine_create();
    double t = 0.0;
    int s1, e = swmm_engine_open(eng, inp, rpt, "API-13_6.out", NULL);
    if (!e) e = swmm_engine_initialize(eng);
    if (!e) e = swmm_engine_start(eng, 0);
    s1 = swmm_subcatch_index(eng, "S1");
    while (!e)
    {
        if (useApi)
            e = swmm_forcing_subcatch_rainfall(eng, s1, (t < 0.5 / 24.0) ? 2.0 : 0.0,
                                               SWMM_FORCING_OVERRIDE, SWMM_FORCING_PERSIST);
        if (!e) e = swmm_engine_step(eng, &t);
        if (t <= 0.0) break;
    }
    if (!e) e = swmm_engine_end(eng);
    if (!e) e = swmm_engine_report(eng);
    swmm_engine_close(eng);
    swmm_engine_destroy(eng);
    if (e) return e;
    return readRunoffContinuity(rpt, precip, err) ? -1 : 0;
}

int main(void)
{
    struct { const char *name, *inp, *rpt; int api; } cases[] = {
        { "rain from the gage (reference)", "API-13_gage-rain.inp",  "API-13_gage-rain6.rpt",  0 },
        { "forcing, gage interval 30 min",  "API-13_gage30min.inp",  "API-13_gage30min6.rpt",  1 },
        { "forcing, gage interval 1 h",     "API-13_gage1h.inp",     "API-13_gage1h6.rpt",     1 },
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
}
