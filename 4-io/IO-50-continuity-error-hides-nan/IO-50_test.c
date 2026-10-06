/*
 * IO-50: a continuity error computed from NaN totals is reported as 0.000 %.
 *
 * massbal_getFlowError() (and the runoff, groundwater, loading and quality
 * versions) start from pctError = 0.0 and overwrite it only in one of three
 * branches: |in - out| < tol, in > 0, out > 0. Every comparison with NaN is
 * false, so when both totals are NaN the error keeps its initial 0.0. The
 * report then prints "Continuity Error (%) ..... 0.000" under rows that read
 * "nan", and swmm_getMassBalErr() returns 0.
 *
 * Any NaN that reaches the ledger shows it (NUM-01 and IO-01 are two sources
 * from the input file). This test uses the most direct one, the toolkit API:
 * swmm_setValue(swmm_NODE_LATFLOW, ...) accepts NaN, and junction J1 gets a
 * NaN lateral inflow for one routing step (step 10 of 240). From then on
 * J1's depth and everything downstream of it are NaN, so the Flow Routing
 * and Quality Routing Continuity tables both have NaN rows.
 *
 * Correct behaviour: if a term of a continuity table is NaN, its continuity
 * error must not read as a number (it must be NaN as well), both in the
 * report and from swmm_getMassBalErr(). A finite ledger is also accepted
 * (that is what a fix that rejected the NaN inflow would give).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define RPT "IO-50.rpt"

/* Reads the continuity table that follows the line containing `title`:
 * counts the rows that print nan and returns the Continuity Error value. */
static int readTable(const char *title, int *nanRows, double *pct)
{
    char line[256];
    FILE *f = fopen(RPT, "r");
    *nanRows = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f) && !strstr(line, title)) ;
    printf("%s table of %s:\n", title, RPT);
    while (fgets(line, sizeof line, f))
    {
        char *dots = strstr(line, "..");
        if (!dots) continue;
        printf("%s", line);
        if (strstr(line, "Continuity Error"))
        {
            while (*dots == '.') dots++;
            *pct = strtod(dots, NULL);
            fclose(f);
            return 1;
        }
        if (strstr(line, "nan")) (*nanRows)++;
    }
    fclose(f);
    return 0;
}

/* Returns 1 if the table is consistent: NaN rows give a NaN error. */
static int check(const char *what, int nanRows, double rptErr, double apiErr)
{
    printf("  nan rows: %d, error in report: %g, from swmm_getMassBalErr(): %g\n\n",
           nanRows, rptErr, apiErr);
    if (nanRows > 0 && (!isnan(rptErr) || !isnan(apiErr)))
    {
        printf("FAIL: %s has %d nan rows but its continuity error reads "
               "%.3f %% in the report and %.3f %% from swmm_getMassBalErr()\n",
               what, nanRows, rptErr, apiErr);
        return 0;
    }
    if (nanRows == 0 && (isnan(rptErr) || isnan(apiErr)))
    {
        printf("FAIL: %s is finite but its continuity error is NaN\n", what);
        return 0;
    }
    return 1;
}

int main(void)
{
    double elapsed = 0.0, flowPct = 0.0, qualPct = 0.0;
    int err, step = 0, j1, flowNan = 0, qualNan = 0;
    float runoffErr = 0.0f, flowErr = 0.0f, qualErr = 0.0f;

    err = swmm_open("IO-50_nan-inflow.inp", RPT, "IO-50.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        step++;
        /* a NaN lateral inflow at J1 for one routing step */
        if (step == 10) swmm_setValue(swmm_NODE_LATFLOW, j1, NAN);
        if (step == 11) swmm_setValue(swmm_NODE_LATFLOW, j1, 0.0);
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    if (!readTable("Flow Routing Continuity", &flowNan, &flowPct) ||
        !readTable("Quality Routing Continuity", &qualNan, &qualPct))
    {
        printf("FAIL: a continuity table is missing from %s\n", RPT);
        return 1;
    }
    printf("\n");
    if (!check("Flow Routing Continuity", flowNan, flowPct, flowErr)) return 1;
    if (!check("Quality Routing Continuity", qualNan, qualPct, qualErr)) return 1;
    if (flowNan + qualNan == 0)
        printf("PASS: the ledgers are finite (no NaN reached them)\n");
    else
        printf("PASS: the flow (%d nan rows) and quality (%d nan rows) continuity "
               "errors are reported as nan, not as a number\n", flowNan, qualNan);
    return 0;
}
