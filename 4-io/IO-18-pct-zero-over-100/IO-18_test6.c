/*
 * IO-18 for 6.0.0: [SUBAREAS] accepts a PctZero above 100 %.
 *
 * PctZero is the percent of the impervious area that has no depression
 * storage, so only 0 to 100 has a meaning. The deck uses 150. SWMM gives the
 * impervious sub-area with depression storage an area fraction of
 * 0.5 x (1 - 150/100) = -0.25 and runs, with a -25 % runoff continuity error.
 *
 * Correct behaviour: the input is rejected with an input error. The test
 * passes if the project fails to open, initialize or start, and prints the
 * engine's error messages. If it runs, the test prints the runoff continuity table and
 * fails.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

/* the inches column of the report line containing label */
static double rptInches(const char* rpt, const char* label)
{
    char line[512];
    double a = 0, b = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, label);
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        sscanf(p, "%lf %lf", &a, &b);
        break;
    }
    fclose(f);
    return b;
}

/* the runoff continuity error (%) in the report */
static double rptPercent(const char* rpt)
{
    char line[512];
    double a = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, "Continuity Error (%)");
        if (!p) continue;
        p += strlen("Continuity Error (%)");
        p += strspn(p, " .");
        sscanf(p, "%lf", &a);
        break;
    }
    fclose(f);
    return a;
}

int main(void)
{
    double t = 0.0;
    SWMM_Engine e = swmm_engine_create();
    int err = swmm_engine_open(e, "IO-18_pctzero-150.inp", "IO-18_6.rpt", "IO-18_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (err)
    {
        int i, n = swmm_get_error_count(e);
        printf("PctZero = 150 rejected, error code %d; the engine says:\n", err);
        for (i = 0; i < n; i++) printf("  %s\n", swmm_get_error_at(e, i));
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        printf("PASS: a PctZero above 100 is reported as an input error\n");
        return 0;
    }
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    printf("PctZero = 150 accepted; the run gives (inches):\n");
    printf("  Total precipitation %8.3f\n", rptInches("IO-18_6.rpt", "Total Precipitation"));
    printf("  Infiltration loss   %8.3f\n", rptInches("IO-18_6.rpt", "Infiltration Loss"));
    printf("  Surface runoff      %8.3f\n", rptInches("IO-18_6.rpt", "Surface Runoff"));
    printf("  Final storage       %8.3f\n", rptInches("IO-18_6.rpt", "Final Storage"));
    printf("  Continuity error (%%) %7.3f\n", rptPercent("IO-18_6.rpt"));
    printf("FAIL: PctZero = 150 is accepted and the run loses track of the water "
           "(runoff continuity error %.3f %%)\n", rptPercent("IO-18_6.rpt"));
    return 1;
}
