/*
 * IO-18: [SUBAREAS] accepts a PctZero above 100 %.
 *
 * PctZero is the percent of the impervious area that has no depression
 * storage, so only 0 to 100 has a meaning. The deck uses 150. SWMM gives the
 * impervious sub-area with depression storage an area fraction of
 * 0.5 x (1 - 150/100) = -0.25 and runs, reporting a negative final surface
 * storage.
 *
 * Correct behaviour: the input is rejected with an input error. The test
 * passes if the project fails to open or start, and prints the error lines of
 * the report. If it runs, the test prints the runoff continuity table and
 * fails.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

static void printLines(const char* rpt, const char* key)
{
    char line[512];
    FILE* f = fopen(rpt, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f))
        if (strstr(line, key)) printf("%s", line);
    fclose(f);
}

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

int main(void)
{
    double elapsed = 0.0;
    int err;

    err = swmm_open("IO-18_pctzero-150.inp", "IO-18.rpt", "IO-18.out");
    if (!err) err = swmm_start(1);
    if (err)
    {
        swmm_close();
        printf("PctZero = 150 rejected, error code %d; the report says:\n", err);
        printLines("IO-18.rpt", "ERROR");
        printf("PASS: a PctZero above 100 is reported as an input error\n");
        return 0;
    }
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    printf("PctZero = 150 accepted; the run gives (inches):\n");
    printf("  Total precipitation %8.3f\n", rptInches("IO-18.rpt", "Total Precipitation"));
    printf("  Infiltration loss   %8.3f\n", rptInches("IO-18.rpt", "Infiltration Loss"));
    printf("  Surface runoff      %8.3f\n", rptInches("IO-18.rpt", "Surface Runoff"));
    printf("  Final storage       %8.3f\n", rptInches("IO-18.rpt", "Final Storage"));
    printf("FAIL: PctZero = 150 is accepted and gives the impervious sub-area with "
           "depression storage a negative area (final storage %.3f in)\n",
           rptInches("IO-18.rpt", "Final Storage"));
    return 1;
}
