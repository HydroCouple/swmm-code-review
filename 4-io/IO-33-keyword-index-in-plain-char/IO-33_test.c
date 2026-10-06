/*
 * IO-33: [FILES] mode and [REPORT] keyword indices are kept in a plain char.
 *
 * iface_readFileParams() and report_readOptions() store findmatch()'s result
 * in a char and test it with k < 0:
 *     char k;  k = (char)findmatch(tok[0], FileModeWords);
 *     if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);
 * findmatch() returns -1 for an unknown word. Where plain char is unsigned
 * (Linux on ARM/aarch64, PowerPC, s390x) (char)-1 is 255, the test is false,
 * and the misspelt word is accepted: a [FILES] line with a misspelt mode is
 * dropped without a message, and a misspelt [REPORT] keyword is reported as
 * an error in the wrong token.
 *
 * Correct behaviour: each misspelt keyword is refused with ERROR 205 naming
 * it: "SVAE" in IO-33_files-typo.inp ([FILES] SVAE HOTSTART "IO-33.hsf") and
 * "CONTINUTY" in IO-33_report-typo.inp ([REPORT] CONTINUTY YES), and
 * "HOTSTRAT" in IO-33_files-type-typo.inp ([FILES] SAVE HOTSTRAT, a check
 * on an int index that every platform passes).
 *
 * On a platform where char is signed (x86, x86-64, Windows, macOS) the engine
 * behaves correctly and this test passes; it fails where char is unsigned.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

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
    double t = 0.0;
    int err, bad;
    FILE *hsf;

    remove("IO-33.hsf");
    err = swmm_open(inp, "IO-33.rpt", "IO-33.out");
    if (!err)
    {
        err = swmm_start(0);
        while (!err)
        {
            err = swmm_step(&t);
            if (!(t > 0.0)) break;
        }
        swmm_end();
        err = 0;   /* the deck was accepted */
    }
    swmm_close();
    firstError("IO-33.rpt", msg, sizeof msg);
    hsf = fopen("IO-33.hsf", "rb");
    printf("%-22s open code %3d, %s%s\n", inp, err, msg[0] ? msg : "no error",
           hsf ? " (IO-33.hsf written)" : "");
    if (hsf) fclose(hsf);
    bad = err == 0 || strstr(msg, "ERROR 205") == NULL || strstr(msg, word) == NULL;
    return bad;
}

int main(void)
{
    int bad1, bad2, bad3;
    printf("plain char is %s in this build\n", (char)-1 < 0 ? "signed" : "unsigned");
    bad1 = check("IO-33_files-typo.inp", "SVAE");
    bad2 = check("IO-33_report-typo.inp", "CONTINUTY");
    bad3 = check("IO-33_files-type-typo.inp", "HOTSTRAT");
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
