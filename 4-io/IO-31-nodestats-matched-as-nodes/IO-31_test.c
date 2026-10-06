/*
 * IO-31: the deprecated [REPORT] keyword NODESTATS is read as NODES.
 *
 * report_readOptions() is meant to accept "NODESTATS YES|NO" and ignore it
 * (report.c: "case 9: return 0; // NODESTATS deprecated"). It looks the
 * keyword up with findmatch(), which accepts any token that starts with a
 * keyword, and ReportWords lists NODE before NODESTATS. "NODESTATS" therefore
 * matches NODE, the line is read as a node list, and "YES" is looked up as a
 * node name: ERROR 209 "undefined object YES".
 *
 * Correct: IO-31_nodestats.inp, a valid model whose [REPORT] section has a
 * NODESTATS YES line, opens and runs without an error.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* copies the first ERROR line of a report into msg */
static void firstError(const char *rpt, char *msg, int len)
{
    char line[256];
    FILE *f = fopen(rpt, "r");
    msg[0] = 0;
    if (!f) return;
    while (fgets(line, sizeof line, f))
    {
        char *p = strstr(line, "ERROR");
        if (p)
        {
            snprintf(msg, len, "%s", p);
            msg[strcspn(msg, "\r\n")] = 0;
            break;
        }
    }
    fclose(f);
}

int main(void)
{
    double elapsed = 0.0;
    char msg[256] = "";
    int err, openErr, steps = 0;

    openErr = err = swmm_open("IO-31_nodestats.inp", "IO-31.rpt", "IO-31.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        steps++;
    }
    if (!err) err = swmm_end();
    swmm_close();
    if (err) firstError("IO-31.rpt", msg, sizeof msg);

    printf("[REPORT] NODESTATS YES: swmm_open returned %d, run error %d, %d routing steps\n",
           openErr, err, steps);
    if (err)
    {
        printf("FAIL: the deck is rejected with error %d; the report says \"%s\"\n", err, msg);
        return 1;
    }
    printf("PASS: the deprecated NODESTATS line is accepted and the model runs\n");
    return 0;
}
