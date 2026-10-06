/*
 * IO-39: 5.3.0's input warnings are not counted, and the unknown-section
 * warning is written twice.
 *
 * IO-39_typos.inp has two problems: a misspelt [OPTIONS] keyword
 * (FLOW_ROUTNG) and a section no SWMM version defines ([FOO]). 5.2.4 rejects
 * both as input errors (ERROR 205). 5.3.0 turned them into warnings that are
 * written with report_writeLine(), which, unlike report_writeWarningMsg(),
 * does not increment the Warnings counter returned by swmm_getWarnings()
 * (and used by the command-line program for "There are warnings"). The
 * input file is read twice (counting pass, then data pass) and each pass
 * writes the unknown-section warning.
 *
 * Correct: every problem is reported once, either as an input error that
 * stops the run (5.2.4) or as a warning that is counted: 2 WARNING lines in
 * the report and swmm_getWarnings() = 2.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* counts the report lines that contain the given word */
static int countLines(const char *rpt, const char *word)
{
    char line[512];
    int n = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) if (strstr(line, word)) n++;
    fclose(f);
    return n;
}

int main(void)
{
    double elapsed = 0.0;
    int err, openErr, nWarn = 0, nRptWarn, nRptErr;

    openErr = err = swmm_open("IO-39_typos.inp", "IO-39.rpt", "IO-39.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    nWarn = swmm_getWarnings();
    swmm_close();
    nRptWarn = countLines("IO-39.rpt", "WARNING");
    nRptErr = countLines("IO-39.rpt", "ERROR");

    printf("FLOW_ROUTNG typo + unknown [FOO] section (2 problems):\n");
    printf("  swmm_open error         %d\n", openErr);
    printf("  ERROR lines in report   %d\n", nRptErr);
    printf("  WARNING lines in report %d\n", nRptWarn);
    printf("  swmm_getWarnings()      %d\n", nWarn);

    if (openErr)
    {
        if (nRptErr >= 1)
        {
            printf("PASS: the problems are reported as input errors and the run is stopped\n");
            return 0;
        }
        printf("FAIL: the open failed (error %d) without an input error in the report\n", openErr);
        return 1;
    }
    if (nRptWarn != 2 || nWarn != 2)
    {
        printf("FAIL: 2 problems give %d WARNING lines in the report and "
               "swmm_getWarnings() = %d (both should be 2)\n", nRptWarn, nWarn);
        return 1;
    }
    printf("PASS: each problem is written once as a warning and counted\n");
    return 0;
}
