/*
 * IO-05: input lines longer than the 1024-character buffer are split.
 *
 * input_countObjects() and input_readData() read the file with
 * fgets(line, MAXLINE, ...), which returns at most MAXLINE-1 = 1023
 * characters. The rest of a longer line comes back from the next fgets() as
 * if it were a line of its own, and is parsed as data. The ERR_LINE_LENGTH
 * check (strlen(line) >= MAXLINE) can never be true.
 *
 * Correct behaviour (Appendix A of the manual: "ERROR 201: too many characters
 * in input line. A line in the input file cannot exceed 1024 characters."):
 *  - IO-05_long-comment.inp: a 1151-character comment line is a comment. The
 *    deck must open and run, with its 2 nodes and 1 link.
 *  - IO-05_long-data-line.inp: a 1098-character [TIMESERIES] line is too long.
 *    The deck must be refused with ERROR 201, naming that line (line 37),
 *    not with an error about a fragment of it.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* copy the first line of the report that contains "ERROR" into msg */
static void firstError(const char *rpt, char *msg, int n)
{
    char buf[2048];
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

int main(void)
{
    char msg[200];
    double t = 0.0;
    int err, nNodes = -1, nLinks = -1, bad1, bad2;

    /* 1. long comment line */
    err = swmm_open("IO-05_long-comment.inp", "IO-05a.rpt", "IO-05a.out");
    if (!err)
    {
        nNodes = swmm_getCount(swmm_NODE);
        nLinks = swmm_getCount(swmm_LINK);
        err = swmm_start(0);
        while (!err)
        {
            err = swmm_step(&t);
            if (!(t > 0.0)) break;
        }
        swmm_end();
    }
    swmm_close();
    firstError("IO-05a.rpt", msg, sizeof msg);
    bad1 = err != 0 || nNodes != 2 || nLinks != 1;
    printf("long comment line:   error code %d, %d nodes, %d links  %s\n", err, nNodes,
           nLinks, msg);

    /* 2. long data line */
    err = swmm_open("IO-05_long-data-line.inp", "IO-05b.rpt", "IO-05b.out");
    swmm_close();
    firstError("IO-05b.rpt", msg, sizeof msg);
    bad2 = err == 0 || strstr(msg, "ERROR 201") == NULL || strstr(msg, "line 37 ") == NULL;
    printf("long data line:      error code %d  %s\n", err, msg);

    if (bad1 || bad2)
    {
        printf("FAIL: %s%s%s\n",
               bad1 ? "the tail of a long comment line is parsed as data" : "",
               bad1 && bad2 ? "; " : "",
               bad2 ? "a long data line is not reported as ERROR 201 at line 37" : "");
        return 1;
    }
    printf("PASS: a long comment line is ignored, and a long data line is refused "
           "with ERROR 201 at its own line number\n");
    return 0;
}
