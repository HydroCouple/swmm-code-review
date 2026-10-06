/*
 * API-18: the legacy command line program exits with status 0 when the run
 * fails.
 *
 * runswmm (5.2.4) / openswmm-legacy (5.3.0) call swmm_run(), print "There are
 * errors." when swmm_getError() returns an error code, and then return 0
 * whatever happened, although main() is documented to return the error
 * status. A batch script or a CI job cannot tell a failed run from a good one
 * without parsing the report.
 *
 * Correct behaviour: the exit status is non-zero exactly when the run failed,
 * i.e. when its report contains an "ERROR" line or no report was written.
 * The test finds the command line program built next to the engine library
 * this test is linked with (dladdr on swmm_open), runs it on four inputs and
 * compares the exit status with the report:
 *   - API-18_clean.inp        a valid model                    -> exit 0
 *   - API-18_bad-number.inp   invalid number (ERROR 211)       -> exit != 0
 *   - API-18_missing.inp      file does not exist (ERROR 303)  -> exit != 0
 *   - API-18_diverges.inp     2e9 cfs inflow; the legacy engines run it to
 *                             the end without an error, so exit 0 is right
 *                             for them (6.0.0 stops it, see API-18_test6.c)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/wait.h>
#include "swmm5.h"

/* the command line program in the build tree of the engine library in use */
static int findCli(char *cli, size_t len)
{
    static const char *rel[] = { "../cli/openswmm-legacy",   /* 5.3.0 tree */
                                 "../run/runswmm" };         /* 5.2.4 tree */
    Dl_info info;
    char lib[4096];
    int i;

    if (!dladdr((void *)swmm_open, &info) || !info.dli_fname) return 0;
    snprintf(lib, sizeof lib, "%s", info.dli_fname);
    for (i = 0; i < 2; i++)
    {
        snprintf(cli, len, "%s/%s", dirname(lib), rel[i]);
        snprintf(lib, sizeof lib, "%s", info.dli_fname);
        if (access(cli, X_OK) == 0) return 1;
    }
    return 0;
}

/* 1 if the report file holds an ERROR line or does not exist */
static int reportHasError(const char *rpt, char *first, size_t len)
{
    char line[1024];
    FILE *f = fopen(rpt, "r");
    first[0] = '\0';
    if (!f) { snprintf(first, len, "(no report written)"); return 1; }
    while (fgets(line, sizeof line, f))
    {
        char *p = strstr(line, "ERROR");
        if (p)
        {
            p[strcspn(p, "\r\n")] = '\0';
            snprintf(first, len, "%.60s", p);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    snprintf(first, len, "(no error)");
    return 0;
}

int main(void)
{
    static const char *deck[4] = { "API-18_clean", "API-18_bad-number",
                                   "API-18_missing", "API-18_diverges" };
    char cli[4200], cmd[8800], rpt[256], first[128];
    int k, bad = 0;

    if (!findCli(cli, sizeof cli))
    {
        printf("FAIL: cannot find the command line program next to the engine library\n");
        return 1;
    }
    printf("command line program: %s\n\n", strrchr(cli, '/') + 1);
    printf("%-20s %-62s %6s  %s\n", "input", "report says", "exit", "verdict");
    for (k = 0; k < 4; k++)
    {
        int status, code, failed, ok;
        snprintf(rpt, sizeof rpt, "%s.rpt", deck[k]);
        remove(rpt);
        snprintf(cmd, sizeof cmd, "\"%s\" %s.inp %s %s.out > %s.log",
                 cli, deck[k], rpt, deck[k], deck[k]);
        fflush(stdout);
        status = system(cmd);
        code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        failed = reportHasError(rpt, first, sizeof first);
        ok = failed ? code != 0 : code == 0;
        if (!ok) bad++;
        printf("%-20s %-62s %6d  %s\n", deck[k], first, code,
               ok ? "ok" : (failed ? "FAILED RUN EXITS 0" : "CLEAN RUN EXITS NON-ZERO"));
    }

    if (bad)
    {
        printf("FAIL: in %d of 4 runs the exit status does not match the report "
               "(a failed run exits with status 0)\n", bad);
        return 1;
    }
    printf("PASS: failed runs exit with a non-zero status and clean runs with 0\n");
    return 0;
}
