/*
 * IO-31 for 6.0.0: the same check as IO-31_test.c through the 6.0.0 API.
 * 6.0.0's [REPORT] handler skips keywords it does not know, so NODESTATS is
 * expected to pass. The legacy description follows.
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
#include "openswmm/engine/openswmm_engine.h"

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
    SWMM_Engine e = swmm_engine_create();

    openErr = err = swmm_engine_open(e, "IO-31_nodestats.inp", "IO-316.rpt", "IO-316.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        steps++;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err) firstError("IO-316.rpt", msg, sizeof msg);

    printf("[REPORT] NODESTATS YES: swmm_engine_open returned %d, run error %d, %d routing steps\n",
           openErr, err, steps);
    if (err)
    {
        printf("FAIL: the deck is rejected with error %d; the report says \"%s\"\n", err, msg);
        return 1;
    }
    printf("PASS: the deprecated NODESTATS line is accepted and the model runs\n");
    return 0;
}
