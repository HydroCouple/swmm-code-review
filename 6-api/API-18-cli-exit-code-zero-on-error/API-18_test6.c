/*
 * API-18 for 6.0.0: does the openswmm command line program exit with a
 * non-zero status when the run fails?
 *
 * Same check as the legacy test, with 6.0.0's CLI (src/cli/main.cpp): the exit
 * status must be non-zero exactly when the run failed, i.e. when its report
 * contains an "ERROR" line or no report was written. The CLI returns the error
 * code when swmm_engine_open/initialize/start fail, but when
 * swmm_engine_step fails it prints "Error at step ...", finishes the report
 * and returns 0. The only run-time step error 6.0.0 raises is its divergence
 * guard (a head or flow beyond 1e9 or not finite), which API-18_diverges.inp
 * trips with a 2e9 cfs inflow ("ERROR 14: the routing solution diverged").
 * The program is found next to the engine library this test is linked with
 * (dladdr on swmm_engine_create).
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/wait.h>
#include "openswmm/engine/openswmm_engine.h"

/* the command line program in the build tree of the engine library in use */
static int findCli(char *cli, size_t len)
{
    static const char *rel[] = { "../cli/openswmm" };        /* 6.0.0 tree */
    Dl_info info;
    char lib[4096];
    int i;

    if (!dladdr((void *)swmm_engine_create, &info) || !info.dli_fname) return 0;
    snprintf(lib, sizeof lib, "%s", info.dli_fname);
    for (i = 0; i < 1; i++)
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
