/*
 * CON-21: the averages in the summary tables are means over routing steps,
 * not over time, so a variable routing step biases them.
 *
 * The deck routes a 0.2-cfs base flow with a 30-minute 40-cfs pulse through
 * a junction (J1), a storage unit (SU1) and a pipe to an outfall (O1) under
 * dynamic wave with VARIABLE_STEP 0.75. The same inflow enters a second
 * wet well (W1) drained by an ideal pump (P1) to a second outfall. The
 * routing step drops from 30 s to a few seconds while the pulse passes, so
 * the high-flow period holds many more steps than its share of the time.
 *
 * The test steps the run and forms time averages of the same end-of-step
 * values that stats.c accumulates, each weighted by the length of its step:
 *   J1 depth and SU1 volume over the whole run;
 *   O1 inflow (the flow of C2, its only inlet) over the steps in which
 *   the outfall flows (>= 0.001 cfs), and the fraction of time it flows;
 *   P1 flow over the steps in which the pump runs (> 0.001 cfs).
 * It then reads the Node Depth, Storage Volume, Outfall Loading and Pumping
 * summaries.
 *
 * Correct behaviour: each reported average equals the time average. The
 * report and the test use the same samples and weights, so the tolerance is
 * the rounding of the column plus 0.5 %. With step-count averaging the
 * averages are 22 % to 43 % high and the flow frequency 1 % high (0.95
 * percentage points, twice the tolerance).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

static FILE *rpt;

/* find the first line after `table` whose first token is `id` */
static int findRow(const char *table, const char *id, char *row)
{
    char line[512], name[64];
    int in = 0;
    rewind(rpt);
    while (fgets(line, sizeof line, rpt))
    {
        if (strstr(line, table)) in = 1;
        if (in && sscanf(line, "%63s", name) == 1 && strcmp(name, id) == 0)
        {
            strcpy(row, line);
            return 1;
        }
    }
    return 0;
}

static int check(const char *what, double rpt, double want, double rounding)
{
    int ok = fabs(rpt - want) <= rounding + 0.005 * fabs(want);
    printf("%-28s %10.3f %10.3f %+8.1f %%%s\n", what, rpt, want,
           want != 0.0 ? (rpt - want) / want * 100.0 : 0.0, ok ? "" : "   <--");
    return ok;
}

int main(void)
{
    double elapsed = 0.0, prev = 0.0, dt, T = 0.0, x;
    double yJ1 = 0.0, vSU1 = 0.0, qO1 = 0.0, tO1 = 0.0, qP1 = 0.0, tP1 = 0.0;
    double v[12];
    char row[512], s1[64], s2[64];
    int err, j1, su1, c2, p1, n, ok = 1;

    err = swmm_open("CON-21_variable-step.inp", "CON-21.rpt", "CON-21.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    su1 = swmm_getIndex(swmm_NODE, "SU1");
    c2 = swmm_getIndex(swmm_LINK, "C2");
    p1 = swmm_getIndex(swmm_LINK, "P1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        dt = (elapsed - prev) * 86400.0;
        prev = elapsed;
        T += dt;
        yJ1 += swmm_getValue(swmm_NODE_DEPTH, j1) * dt;
        vSU1 += swmm_getValue(swmm_NODE_VOLUME, su1) * dt;
        x = swmm_getValue(swmm_LINK_FLOW, c2);
        if (x >= 0.001) { qO1 += x * dt; tO1 += dt; }
        x = fabs(swmm_getValue(swmm_LINK_FLOW, p1));
        if (x > 0.001) { qP1 += x * dt; tP1 += dt; }
    }
    swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    rpt = fopen("CON-21.rpt", "r");
    if (!rpt) { printf("FAIL: cannot read CON-21.rpt\n"); return 1; }
    printf("                                 report  time avg.  report error\n");

    /* Node Depth Summary: name type avgDepth ... */
    n = findRow("Node Depth Summary", "J1", row) ? sscanf(row, "%63s %63s %lf", s1, s2, &v[0]) : 0;
    if (n != 3) { printf("FAIL: no J1 row in the Node Depth Summary\n"); return 1; }
    ok &= check("J1 Average Depth (ft)", v[0], yJ1 / T, 0.005);

    /* Storage Volume Summary: name avgVol(1000 ft3) avgPcnt ... */
    n = findRow("Storage Volume Summary", "SU1", row) ? sscanf(row, "%63s %lf", s1, &v[0]) : 0;
    if (n != 2) { printf("FAIL: no SU1 row in the Storage Volume Summary\n"); return 1; }
    ok &= check("SU1 Avg Volume (1000 ft3)", v[0], vSU1 / T / 1000.0, 0.0005);

    /* Outfall Loading Summary: name freq% avgFlow maxFlow volume */
    n = findRow("Outfall Loading Summary", "O1", row) ? sscanf(row, "%63s %lf %lf", s1, &v[0], &v[1]) : 0;
    if (n != 3) { printf("FAIL: no O1 row in the Outfall Loading Summary\n"); return 1; }
    ok &= check("O1 Flow Freq (%)", v[0], tO1 / T * 100.0, 0.005);
    ok &= check("O1 Avg Flow (cfs)", v[1], qO1 / tO1, 0.005);

    /* Pumping Summary: name pctUtilized startUps minFlow avgFlow ... */
    n = findRow("Pumping Summary", "P1", row) ?
        sscanf(row, "%63s %lf %lf %lf %lf", s1, &v[0], &v[1], &v[2], &v[3]) : 0;
    if (n != 5) { printf("FAIL: no P1 row in the Pumping Summary\n"); return 1; }
    ok &= check("P1 Avg Flow (cfs)", v[3], qP1 / tP1, 0.005);
    fclose(rpt);

    if (!ok)
    {
        printf("FAIL: the summary averages are not time averages (marked rows)\n");
        return 1;
    }
    printf("PASS: the summary averages are time averages over the run\n");
    return 0;
}
