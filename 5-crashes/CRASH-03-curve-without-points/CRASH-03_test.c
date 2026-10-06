/*
 * CRASH-03: table_validate() dereferences a NULL entry pointer for a curve or
 * time series that has no data points.
 *
 * CRASH-03_empty-curve.inp declares the storage curve CV1 with its name and
 * type only (the reader accepts that line, expecting points to follow), and
 * CRASH-03_empty-timeseries.inp has a time series whose only line lacks the
 * value, so the series gets no entries. In both cases project_validate()
 * calls table_validate(), whose loop over table_getNextEntry() reads
 * thisEntry->next with thisEntry still NULL.
 *
 * Correct behaviour: swmm_open() returns without crashing and rejects the
 * input with the error the engine uses for invalid table data: ERROR 171 for
 * the curve, ERROR 173 for the time series.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* returns 1 if the report file contains the given text */
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
    int err, found;
    printf("%s:\n", inp);
    err = swmm_open(inp, rpt, out);
    swmm_close();
    printf("    swmm_open returned %d\n", err);
    sprintf(text, "ERROR %d", expect);
    found = reportHas(rpt, text);
    return err == expect && found;
}

int main(void)
{
    int ok1 = check("CRASH-03_empty-curve.inp", "CRASH-03_curve.rpt", "CRASH-03_curve.out", 171);
    int ok2 = check("CRASH-03_empty-timeseries.inp", "CRASH-03_ts.rpt", "CRASH-03_ts.out", 173);
    if (!ok1 || !ok2)
    {
        printf("FAIL: a table without data is not rejected (curve %s, time series %s)\n",
               ok1 ? "ok" : "accepted", ok2 ? "ok" : "accepted");
        return 1;
    }
    printf("PASS: the curve and the time series without data are rejected "
           "with ERROR 171 and 173, without a crash\n");
    return 0;
}
