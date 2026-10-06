/*
 * BND-19 for 6.0.0: same deck and checks as BND-19_test.c, run through the
 * 6.0.0 C API (DefaultReportPlugin prints ctx.nodes.stat_total_inflow_vol,
 * the counterpart of massbal's NodeInflow, accumulated from START).
 *
 * BND-19: with REPORT_START later than START, the Node Inflow and Outfall
 * Loading summaries put whole-run volumes on the same row as statistics of
 * the reporting period.
 *
 * stats_updateFlowStats() ignores routing steps before REPORT_START, so
 * lateral inflow volume, peak flows, outfall flow frequency, average flow and
 * pollutant load cover only the reporting period. "Total Inflow Volume" and
 * the outfall "Total Volume" print massbal's NodeInflow[], which counts from
 * START.
 *
 * Correct behaviour: each row describes one period. Checked with three
 * identities that hold whatever period the tables cover (BND-19_rpt-start-2h.inp,
 * REPORT_START 02:00, END 06:00, fixed 5 s steps; J1 receives only an external
 * inflow carrying a constant 10 mg/L of TSS):
 *   1. J1 Total Inflow Volume = J1 Lateral Inflow Volume (J1 has no upstream link)
 *   2. O1 Total Volume = Avg Flow x Flow Freq x reporting period (4 h)
 *   3. O1 TSS load / O1 Total Volume = 10 mg/L (kg per 10^6 ltr)
 * Tolerance 3 %: the tables print 3 significant digits and the identities
 * hold to rounding; the bug gives a factor of 3.8.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

#define INP "BND-19_rpt-start-2h.inp"
#define RPT "BND-19_6.rpt"
#define OUT "BND-19_6.out"
#define PERIOD_SEC (4.0 * 3600.0)       /* REPORT_START 02:00 to END 06:00 */

/* find the first line starting with "  <id> " after the line containing <table>
   and split it into whitespace-separated tokens */
static int row(const char *table, const char *id, char tok[][32], int maxTok)
{
    char line[512], key[64];
    int found = 0, n = 0;
    FILE *f = fopen(RPT, "r");
    if (!f) return 0;
    sprintf(key, "  %s ", id);
    while (fgets(line, sizeof(line), f))
    {
        if (!found) { if (strstr(line, table)) found = 1; continue; }
        if (strncmp(line, key, strlen(key)) == 0)
        {
            char *p = strtok(line, " \t\r\n");
            while (p && n < maxTok) { strncpy(tok[n], p, 31); tok[n][31] = 0; n++; p = strtok(NULL, " \t\r\n"); }
            break;
        }
    }
    fclose(f);
    return n;
}

static int off(double a, double b) { return fabs(a - b) > 0.03 * fabs(b); }

int main(void)
{
    char t1[16][32], t2[16][32];
    double t = 0.0, latVol, totVol, freq, avgQ, outVol, load, qVol, conc;
    int err, n1, n2, bad = 0;

    SWMM_Engine e = swmm_engine_create();
    err = swmm_engine_open(e, INP, RPT, OUT, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err) { printf("FAIL: run stopped with error %d\n", err); return 1; }

    /* J1 JUNCTION maxLat maxTot days hr:min latVol totVol balErr */
    n1 = row("Node Inflow Summary", "J1", t1, 16);
    /* O1 freq avgFlow maxFlow totVol TSSload */
    n2 = row("Outfall Loading Summary", "O1", t2, 16);
    if (n1 < 8 || n2 < 6) { printf("FAIL: summary rows not found in %s\n", RPT); return 1; }
    latVol = atof(t1[6]);  totVol = atof(t1[7]);
    freq = atof(t2[1]);    avgQ = atof(t2[2]);  outVol = atof(t2[4]);  load = atof(t2[5]);
    qVol = avgQ * freq / 100.0 * PERIOD_SEC / 1000.0;     /* m3 -> 10^6 ltr */
    conc = load / outVol;                                 /* kg / 10^6 ltr = mg/L */

    printf("Node Inflow Summary, J1:     Lateral Inflow Volume %7.3f   Total Inflow Volume %7.3f  (10^6 ltr)\n",
           latVol, totVol);
    printf("Outfall Loading Summary, O1: Flow Freq %.2f %%  Avg Flow %.3f CMS  Total Volume %.3f  TSS %.3f kg\n",
           freq, avgQ, outVol, load);
    printf("  Avg Flow x Freq x 4 h  = %7.3f 10^6 ltr   (Total Volume %.3f)\n", qVol, outVol);
    printf("  TSS load / Total Volume = %6.2f mg/L     (inflow concentration 10 mg/L)\n", conc);

    if (off(totVol, latVol)) { bad++; printf("  J1: total inflow volume differs from its lateral inflow volume\n"); }
    if (off(outVol, qVol))   { bad++; printf("  O1: total volume differs from average flow x frequency x period\n"); }
    if (off(conc, 10.0))     { bad++; printf("  O1: load / volume differs from the 10 mg/L carried by the flow\n"); }

    if (bad)
    {
        printf("FAIL: %d of 3 summary identities broken (J1 total/lateral inflow volume %.3f/%.3f, "
               "O1 volume %.3f vs avg flow x freq x period %.3f, O1 TSS %.2f mg/L)\n",
               bad, totVol, latVol, outVol, qVol, conc);
        return 1;
    }
    printf("PASS: volumes, flows and loads in the summary rows all cover the reporting period\n");
    return 0;
}
