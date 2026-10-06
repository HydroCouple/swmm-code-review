/*
 * IO-37 for 6.0.0: the same check as IO-37_test.c through the 6.0.0 API.
 * 6.0.0 reads hot start and runoff interface files with read-only streams,
 * so it is expected to pass. It does not implement rainfall interface files
 * (USE RAINFALL is ignored with a warning), so that deck is not run here.
 *
 * The test
 *   1. runs IO-37_save-hotstart.inp, which writes the hot start file
 *      IO-37.hsf, and IO-37_save-interface.inp, which writes the runoff
 *      interface file IO-37.rof;
 *   2. makes the files read-only (chmod 444). The review's harness runs as
 *      root, which ignores permission bits, so when started as root the test
 *      then switches to the unprivileged user 65534 (nobody) and makes the
 *      working folder writable for it;
 *   3. runs IO-37_use-hotstart.inp and IO-37_use-runoff.inp.
 * Correct: both runs open their read-only input file and finish without an
 * error.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#include <sys/stat.h>
#include "openswmm/engine/openswmm_engine.h"

static int runDeck(const char *inp, const char *rpt, const char *out)
{
    double elapsed = 0.0;
    int err;
    SWMM_Engine e = swmm_engine_create();
    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

/* copies the first ERROR line of a report into msg */
static void firstError(const char *rpt, char *msg, int len)
{
    char line[512];
    FILE *f = fopen(rpt, "r");
    msg[0] = 0;
    if (!f) return;
    while (fgets(line, sizeof line, f))
    {
        char *p = strstr(line, "ERROR");
        if (p)
        {
            char *a, *b;
            snprintf(msg, len, "%s", p);
            msg[strcspn(msg, "\r\n")] = 0;
            /* drop the folder part of a file path, which is long and varies */
            a = strchr(msg, '/');
            b = strrchr(msg, '/');
            if (a && b) memmove(a, b + 1, strlen(b + 1) + 1);
            break;
        }
    }
    fclose(f);
}

int main(void)
{
    const char *files[2] = {"IO-37.hsf", "IO-37.rof"};
    const char *decks[2] = {"use-hotstart", "use-runoff"};
    char inp[64], rpt[64], out[64], msg[2][512];
    int i, err, errs[2], nBad = 0;

    /* 1. write the interface files */
    err = runDeck("IO-37_save-hotstart.inp", "IO-37_save-hotstart6.rpt", "IO-37_save-hotstart6.out");
    if (!err) err = runDeck("IO-37_save-interface.inp", "IO-37_save-interface6.rpt",
                            "IO-37_save-interface6.out");
    if (err)
    {
        printf("FAIL: the decks that write the files did not run (error %d)\n", err);
        return 1;
    }

    /* 2. make them read-only, and stop being root (root may write anyway) */
    for (i = 0; i < 2; i++) chmod(files[i], 0444);
    if (geteuid() == 0)
    {
        chmod(".", 0777);
        if (setgroups(0, NULL) != 0 || setgid(65534) != 0 || setuid(65534) != 0)
        {
            printf("FAIL: could not switch from root to user 65534 for the test\n");
            return 1;
        }
    }
    printf("Running as uid %d; interface files writable by this user: %s, %s\n",
           (int)geteuid(), access(files[0], W_OK) == 0 ? "yes" : "no",
           access(files[1], W_OK) == 0 ? "yes" : "no");

    /* 3. read them back */
    for (i = 0; i < 2; i++)
    {
        snprintf(inp, sizeof inp, "IO-37_%s.inp", decks[i]);
        snprintf(rpt, sizeof rpt, "IO-37_%s6.rpt", decks[i]);
        snprintf(out, sizeof out, "IO-37_%s6.out", decks[i]);
        errs[i] = runDeck(inp, rpt, out);
        firstError(rpt, msg[i], sizeof msg[i]);
        if (errs[i]) nBad++;
    }

    printf("  %-14s %-10s %6s   %s\n", "deck", "reads", "error", "first error in report");
    for (i = 0; i < 2; i++)
        printf("  %-14s %-10s %6d   %s\n", decks[i], files[i], errs[i],
               msg[i][0] ? msg[i] : "-");
    printf("  (correct: both read their read-only file and run)\n");

    if (nBad)
    {
        printf("FAIL: %d of 2 read-only interface files cannot be used: they are "
               "opened for writing\n", nBad);
        return 1;
    }
    printf("PASS: read-only hot start and runoff interface files are used\n");
    return 0;
}
