/*
 * IO-52: errors raised after the input has been read, by setting ErrorCode
 * directly, never reach the report file.
 *
 * report_writeErrorCode() writes exactly these errors (101-107, 301-307,
 * 500) to the report, but nothing has called it since release 5.2.0, when
 * swmm_report() stopped doing so. Errors set with report_writeErrorMsg() are
 * written when they happen; errors set with a bare "ErrorCode = ..." are not.
 *
 * Case A: the binary results file cannot be opened (its directory does not
 *         exist). output_open() sets ErrorCode = ERR_OUT_FILE (307) and
 *         swmm_start() returns it. Any input file does this.
 * Case B: a run that stops with ERR_TIMESTEP (107) in execRouting(). The
 *         deck reaches it through the [EVENTS]/RULE_STEP defect BND-10; once
 *         that is fixed the run completes and case B has nothing to check.
 *
 * Correct behaviour: when a run stops with error code N, the report file
 * says so ("ERROR N: ..."), as it does for every other error.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* Returns 1 if the report contains "ERROR <code>". */
static int reportHasError(const char *rpt, int code)
{
    char line[512], tag[32];
    int found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    snprintf(tag, sizeof tag, "ERROR %d:", code);
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, tag))
        {
            found = 1;
            printf("    report: %s", line);
        }
    }
    fclose(f);
    return found;
}

/* Runs inp; returns the first error code (0 if the run completes). */
static int run(const char *inp, const char *rpt, const char *out)
{
    double elapsed = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    if (!err) swmm_report();
    swmm_close();
    return err;
}

static int check(const char *what, const char *rpt, int err)
{
    int found;
    printf("\n%s\n    error code returned: %d\n", what, err);
    if (err == 0)
    {
        printf("    the run completed, nothing to report\n");
        return 1;
    }
    found = reportHasError(rpt, err);
    printf("    \"ERROR %d\" in %s: %s\n", err, rpt, found ? "yes" : "no");
    return found;
}

int main(void)
{
    int errA, errB, okA, okB;

    errA = run("IO-52_simple.inp", "IO-52a.rpt", "no_such_dir/IO-52a.out");
    okA = check("Case A: results file in a directory that does not exist", "IO-52a.rpt", errA);
    errB = run("IO-52_events-rule-step.inp", "IO-52b.rpt", "IO-52b.out");
    okB = check("Case B: [EVENTS] and RULE_STEP (zero routing step)", "IO-52b.rpt", errB);

    if (!okA || !okB)
    {
        printf("FAIL: the run stopped with error");
        if (!okA) printf(" %d (case A)", errA);
        if (!okA && !okB) printf(" and");
        if (!okB) printf(" %d (case B)", errB);
        printf(" but the report does not say so\n");
        return 1;
    }
    printf("PASS: every error that stopped a run is written to its report\n");
    return 0;
}
