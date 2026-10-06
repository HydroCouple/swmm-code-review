/*
 * CRASH-08: closing a project that has a control-rule EXPRESSION crashes when
 * the input had an error found while objects were being counted.
 *
 * ExpressionCount is incremented in the counting pass. If that pass already
 * found an error (a duplicate ID, an invalid option value), createObjects()
 * returns before controls_create() allocates Expression[], and on close
 * controls_delete() loops over ExpressionCount entries of the NULL array.
 *
 * Correct behaviour: swmm_open() returns an input error, the report names it
 * (ERROR 207 for the duplicate junction J1, ERROR 205 for FLOW_UNITS BOGUS),
 * and swmm_close() returns. With the bug swmm_close() dereferences NULL
 * (AddressSanitizer: SEGV in controls_delete), and the report, still
 * buffered, is left empty.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

static int rptHas(const char *rpt, const char *text)
{
    char line[512];
    int found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (!found && fgets(line, sizeof line, f)) found = strstr(line, text) != NULL;
    fclose(f);
    return found;
}

static int check(const char *inp, const char *rpt, const char *out, const char *msg)
{
    int err = swmm_open(inp, rpt, out);
    printf("%-32s swmm_open() returned %d; ", inp, err);
    fflush(stdout);
    swmm_close();                              /* crashes here with the bug */
    printf("swmm_close() returned; report %s \"%s\"\n",
           rptHas(rpt, msg) ? "has" : "does NOT have", msg);
    return err != 0 && rptHas(rpt, msg);
}

int main(void)
{
    int ok1 = check("CRASH-08_expr-duplicate-id.inp", "CRASH-08_dup.rpt", "CRASH-08_dup.out",
                    "ERROR 207");
    int ok2 = check("CRASH-08_expr-bad-option.inp", "CRASH-08_opt.rpt", "CRASH-08_opt.out",
                    "ERROR 205");
    if (!ok1 || !ok2)
    {
        printf("FAIL: an input error was not reported (duplicate ID: %s, bad option: %s)\n",
               ok1 ? "ok" : "missing", ok2 ? "ok" : "missing");
        return 1;
    }
    printf("PASS: both input errors are reported and the projects close cleanly\n");
    return 0;
}
