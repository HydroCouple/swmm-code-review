/*
 * BND-18: with [REPORT] AVERAGES YES and REPORT_START later than START, the
 * first saved period averages every routing step since START.
 *
 * With AVERAGES YES each saved node/link value is the mean of the end-of-step
 * values of the routing steps that ended in that reporting period
 * (output_updateAvgResults / output_saveAvgResults). Periods before
 * REPORT_START are not saved, but output_saveResults() returns before the
 * accumulators are cleared, so the first saved period still holds the sum of
 * every step since START.
 *
 * Correct behaviour, checked here: every saved J1 depth and C1 flow in the
 * .out equals the mean of the values read after each routing step that ended
 * in that period (routing step 5 s, fixed, so no step straddles a report
 * time). Tolerance 0.001 ft / 0.001 cfs: the two means differ only by float
 * rounding (~1e-6); the bug gives 0.26 ft against 0.08 ft.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#define INP "BND-18_avg-late-start.inp"
#define RPT "BND-18.rpt"
#define OUT "BND-18.out"
#define START_DAY 43831.0          /* 01/01/2020, START_TIME 00:00 */
#define STEP      900              /* REPORT_STEP, s */
#define NPER      17               /* report times 0..16 x 15 min (END 04:00) */
#define RS_PER    12               /* REPORT_START 03:00 = report time 12 */

static double sumY[NPER], sumQ[NPER], cnt[NPER];

/* date, J1 depth (node 0) and C1 flow (link 0) of every saved period */
static int read_out(double date[], double y[], double q[])
{
    int head[7], rec[6], np, i, ns, nn;
    long bpp, pos;
    float v;
    FILE *f = fopen(OUT, "rb");
    if (!f) return 0;
    fread(head, 4, 7, f);                 /* magic, version, units, ns, nn, nl, npol */
    fseek(f, -24L, SEEK_END);
    fread(rec, 4, 6, f);                  /* ids, props, results pos, periods, error, magic */
    np = rec[3];
    ns = head[3] * (8 + head[6]);         /* REAL4 values per period for subcatchments */
    nn = head[4] * (6 + head[6]);         /* ... and for nodes */
    bpp = 8 + 4L * (ns + nn + head[5] * (5 + head[6]) + 15);
    for (i = 0; i < np && i < NPER; i++)
    {
        pos = rec[2] + i * bpp;
        fseek(f, pos, SEEK_SET);
        fread(&date[i], 8, 1, f);
        fseek(f, pos + 8 + 4L * ns, SEEK_SET);         /* node 0, NODE_DEPTH */
        fread(&v, 4, 1, f);
        y[i] = v;
        fseek(f, pos + 8 + 4L * (ns + nn), SEEK_SET);  /* link 0, LINK_FLOW */
        fread(&v, 4, 1, f);
        q[i] = v;
    }
    fclose(f);
    return np;
}

int main(void)
{
    double t = 0.0, date[NPER], y[NPER], q[NPER], sy = 0, sq = 0, n = 0;
    int err, j1, c1, np, i, bad = 0;

    err = swmm_open(INP, RPT, OUT);
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    while (!err)
    {
        long s;
        err = swmm_step(&t);
        if (t <= 0.0) break;
        s = (long)floor(t * 86400.0 + 0.5);
        i = (int)((s + STEP - 1) / STEP);         /* period that ends at i*STEP */
        if (i < NPER)
        {
            sumY[i] += swmm_getValue(swmm_NODE_DEPTH, j1);
            sumQ[i] += swmm_getValue(swmm_LINK_FLOW, c1);
            cnt[i] += 1;
        }
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: run stopped with error %d\n", err);
        return 1;
    }
    for (i = 1; i <= RS_PER; i++) { sy += sumY[i]; sq += sumQ[i]; n += cnt[i]; }

    np = read_out(date, y, q);
    printf("Saved period   J1 depth (ft)       C1 flow (cfs)       Steps\n");
    printf("               .out    step mean   .out    step mean\n");
    for (i = 0; i < np; i++)
    {
        int p = (int)floor((date[i] - START_DAY) * 86400.0 / STEP + 0.5);
        double my = -1.0, mq = -1.0;
        int wrong;
        if (p > 0 && p < NPER && cnt[p] > 0) { my = sumY[p] / cnt[p]; mq = sumQ[p] / cnt[p]; }
        else p = 0;
        wrong = fabs(y[i] - my) > 0.001 || fabs(q[i] - mq) > 0.001;
        if (wrong) bad++;
        if (i < 3 || wrong)
            printf("%02d:%02d-%02d:%02d    %6.4f  %6.4f      %6.4f  %6.4f      %4.0f%s\n",
                   (p - 1) * STEP / 3600, (p - 1) * STEP / 60 % 60, p * STEP / 3600,
                   p * STEP / 60 % 60, y[i], my, q[i], mq, cnt[p],
                   wrong ? "  <-- wrong" : "");
    }
    printf("Mean of all %.0f steps from START to 03:00: J1 depth %.4f ft, C1 flow %.4f cfs\n",
           n, sy / n, sq / n);

    if (np < 1 || bad)
    {
        printf("FAIL: %d of %d saved periods are not the mean of their own routing steps "
               "(first period J1 depth %.4f ft, should be %.4f ft)\n",
               bad, np, np > 0 ? y[0] : -1.0, sumY[RS_PER] / cnt[RS_PER]);
        return 1;
    }
    printf("PASS: every saved period is the mean of the routing steps in that period\n");
    return 0;
}
