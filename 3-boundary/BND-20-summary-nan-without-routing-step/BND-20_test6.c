/*
 * BND-20 for 6.0.0: same decks and check as BND-20_test.c, through the 6.0.0
 * C API (swmm_engine_start -> swmm_engine_end -> swmm_engine_report for B).
 * 6.0.0 already guards the node and storage averages (max(report_steps, 1));
 * the remaining divisions are in DefaultReportPlugin.cpp.
 *
 * BND-20: the summary tables print -nan when no routing step falls inside the
 * reporting period.
 *
 * The status report divides by the number of routing steps (ReportStepCount)
 * and the routing time (RoutingTimeSpan) counted after REPORT_START, and the
 * groundwater table by the runoff time, without checking for zero:
 *   Node Depth Summary     avgDepth / ReportStepCount
 *   Storage Volume Summary avgVol / ReportStepCount
 *   Outfall Loading        100 * flowCount / ReportStepCount
 *   Flow Classification    timeInFlowClass / RoutingTimeSpan, timeNormalFlow, timeInletControl
 *   Pumping Summary        utilized / RoutingTimeSpan
 *   Groundwater Summary    avgUpperMoist / runoff time, avgWaterTable / runoff time
 * Both counters stay 0 when
 *   A. [EVENTS] leave the reporting period without any event (BND-20_events.inp:
 *      one event 00:00-02:00, REPORT_START 03:00, END 04:00), or
 *   B. a toolkit client calls swmm_start() and then swmm_end() and swmm_report()
 *      without a single swmm_step() (BND-20_no-step.inp, the same network).
 *
 * Correct behaviour: every number in the report is finite (a statistic over
 * no time steps is 0). The test scans each .rpt for tokens that parse
 * completely as NaN or infinity.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* count tokens that are NaN or infinite; print the first few lines with one */
static int count_nonfinite(const char *rpt)
{
    char line[1024], copy[1024];
    int n = 0, shown = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f))
    {
        int inLine = 0;
        char *tok;
        strcpy(copy, line);
        for (tok = strtok(copy, " \t\r\n"); tok; tok = strtok(NULL, " \t\r\n"))
        {
            char *end;
            double x = strtod(tok, &end);
            if (end != tok && *end == '\0' && !isfinite(x)) { n++; inLine++; }
        }
        if (inLine && shown < 6) { printf("    %s", line); shown++; }
    }
    fclose(f);
    return n;
}

static int run(const char *inp, const char *rpt, const char *out, int stepping)
{
    double t = 0.0;
    SWMM_Engine e = swmm_engine_create();
    int err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err && stepping)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    int errA, errB, nA, nB;

    printf("A. [EVENTS] end before REPORT_START, full run:\n");
    errA = run("BND-20_events.inp", "BND-20_events6.rpt", "BND-20_events6.out", 1);
    nA = count_nonfinite("BND-20_events6.rpt");
    printf("   error %d, %d NaN/inf values in the report\n", errA, nA);

    printf("B. swmm_engine_start() then swmm_engine_end() with no swmm_engine_step():\n");
    errB = run("BND-20_no-step.inp", "BND-20_no-step6.rpt", "BND-20_no-step6.out", 0);
    nB = count_nonfinite("BND-20_no-step6.rpt");
    printf("   error %d, %d NaN/inf values in the report\n", errB, nB);

    if (errA || errB || nA != 0 || nB != 0)
    {
        printf("FAIL: the status report contains %d NaN/inf values with no routing step "
               "in the reporting period (A: %d, B: %d; errors %d, %d)\n",
               nA + nB, nA, nB, errA, errB);
        return 1;
    }
    printf("PASS: no NaN or infinite values in either report\n");
    return 0;
}
