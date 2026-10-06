/*
 * IO-52 for 6.0.0: an error that stops a run must be written to its report.
 *
 * Legacy 5.2.4/5.3.0 lose errors that are set directly in ErrorCode after
 * the input has been read (results file cannot be opened, zero time step).
 * 6.0.0 writes the error of a failed run into the report through its report
 * plugin, so this test is expected to pass unpatched.
 *
 * Case A: the binary results file is in a directory that does not exist.
 * Case B: the [EVENTS]/RULE_STEP deck that stops 5.2.4 and 5.3.0 with
 *         ERROR 107 (6.0.0 completes it, so there is nothing to report).
 *
 * Correct behaviour: when a run stops with an error, the report contains the
 * error message that the API returns for it.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

static char lastMsg[512];

/* Runs inp; returns the first error code (0 if the run completes). */
static int run(const char *inp, const char *rpt, const char *out)
{
    double t = 0.0;
    SWMM_Engine e = swmm_engine_create();
    int rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
    }
    lastMsg[0] = '\0';
    if (rc)
    {
        const char *m = swmm_get_last_error_msg(e);
        snprintf(lastMsg, sizeof lastMsg, "%s", m ? m : "");
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

/* Returns 1 if the report contains msg. */
static int reportHas(const char *rpt, const char *msg)
{
    char line[1024];
    int found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f || !msg[0]) { if (f) fclose(f); return 0; }
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, msg))
        {
            found = 1;
            printf("    report: %s", line);
        }
    }
    fclose(f);
    return found;
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
    printf("    error message: %s\n", lastMsg);
    found = reportHas(rpt, lastMsg);
    printf("    message in %s: %s\n", rpt, found ? "yes" : "no");
    return found;
}

int main(void)
{
    int errA, errB, okA, okB;

    errA = run("IO-52_simple.inp", "IO-52a6.rpt", "no_such_dir/IO-52a6.out");
    okA = check("Case A: results file in a directory that does not exist", "IO-52a6.rpt", errA);
    errB = run("IO-52_events-rule-step.inp", "IO-52b6.rpt", "IO-52b6.out");
    okB = check("Case B: [EVENTS] and RULE_STEP", "IO-52b6.rpt", errB);

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
