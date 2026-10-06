/*
 * IO-33 for 6.0.0: misspelt [FILES] mode and [REPORT] keyword.
 *
 * handle_files() skips a line whose mode is not SAVE or USE ("skip silently
 * (legacy behaviour)") and ignores an unknown file type; handle_report()
 * ignores an unknown keyword. Legacy SWMM reports all three as ERROR 205
 * (on platforms where char is signed; see IO-33_test.c).
 *
 * Correct behaviour, as in IO-33_test.c: each deck is refused with ERROR 205
 * naming the misspelt word ("SVAE", "CONTINUTY", "HOTSTRAT").
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

static void firstError(const char *rpt, char *msg, int n)
{
    char buf[512];
    FILE *f = fopen(rpt, "r");
    msg[0] = '\0';
    if (!f) return;
    while (fgets(buf, sizeof buf, f))
    {
        char *e = strstr(buf, "ERROR");
        if (e)
        {
            strncpy(msg, e, n - 1);
            msg[n - 1] = '\0';
            msg[strcspn(msg, "\r\n")] = '\0';
            break;
        }
    }
    fclose(f);
}

static int check(const char *inp, const char *word)
{
    char msg[200];
    int err, bad;
    FILE *hsf;

    remove("IO-33.hsf");
    err = swmm_engine_run(inp, "IO-33_6.rpt", "IO-33_6.out", NULL);
    firstError("IO-33_6.rpt", msg, sizeof msg);
    hsf = fopen("IO-33.hsf", "rb");
    printf("%-22s run code %3d, %s%s\n", inp, err, msg[0] ? msg : "no error",
           hsf ? " (IO-33.hsf written)" : "");
    if (hsf) fclose(hsf);
    bad = err == 0 || strstr(msg, "ERROR 205") == NULL || strstr(msg, word) == NULL;
    return bad;
}

int main(void)
{
    int bad1 = check("IO-33_files-typo.inp", "SVAE");
    int bad2 = check("IO-33_report-typo.inp", "CONTINUTY");
    int bad3 = check("IO-33_files-type-typo.inp", "HOTSTRAT");
    if (bad1 || bad2 || bad3)
    {
        printf("FAIL: a misspelt keyword is not reported as ERROR 205 naming it%s%s%s\n",
               bad1 ? " ([FILES] SVAE)" : "", bad2 ? " ([REPORT] CONTINUTY)" : "",
               bad3 ? " ([FILES] HOTSTRAT)" : "");
        return 1;
    }
    printf("PASS: misspelt [FILES] mode and type and [REPORT] keyword are refused "
           "with ERROR 205 naming the word\n");
    return 0;
}
