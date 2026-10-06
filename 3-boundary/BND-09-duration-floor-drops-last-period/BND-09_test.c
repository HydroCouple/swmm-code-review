/*
 * BND-09: the run stops 1 s early and loses its last reporting period.
 *
 * TotalDuration is floor()ed from a product of day fractions:
 *   5.2.4: floor((EndDateTime - StartDateTime) * 86400)
 *   5.3.0: floor((EndDate - StartDate) * 86400 + (EndTime - StartTime) * 86400)
 * For many start/end times the product is 1 ulp below the whole number of
 * seconds (e.g. 3779.9999999999995 for 01:03), floor() drops a whole second,
 * the routing clock stops at T - 1 s and the report time T is never reached.
 *
 * Correct behaviour: a run from START to END with REPORT_STEP 1 min writes
 * (END - START) / 1 min reporting periods to the .out file and its last
 * routing step ends at END. The test rewrites START_TIME/END_TIME of
 * BND-09_end0103.inp for every whole minute from 00:01 to 02:00 after a 00:00
 * start, plus the whole-hour runs 00:00-10:00 and 01:00-08:00, runs each case
 * and reads the period count from the .out file's closing records (the
 * legacy swmm_step() returns 0 on the last step, so the time it ends at is
 * not observable here; BND-09_test6.c also checks it). Exact comparison:
 * the bug costs a whole period.
 *
 * With 5.3.0's toolkit header the same sweep is repeated with the end date
 * set through swmm_setValueExpanded(swmm_SYSTEM, swmm_ENDDATE, ...), which
 * uses the same formula.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "swmm5.h"

#define DECK  "BND-09_end0103.inp"
#define CASE  "BND-09_case.inp"
#define RPT   "BND-09_case.rpt"
#define OUT   "BND-09_case.out"

static char deck[8192];

/* write CASE: DECK with START_TIME / REPORT_START_TIME / END_TIME replaced */
static void write_case(int startMin, int endMin)
{
    FILE *f = fopen(CASE, "w");
    char *line = deck, *eol;
    while (*line)
    {
        eol = strchr(line, '\n');
        if (!eol) eol = line + strlen(line);
        if (strncmp(line, "START_TIME ", 11) == 0)
            fprintf(f, "START_TIME           %02d:%02d:00\n", startMin / 60, startMin % 60);
        else if (strncmp(line, "REPORT_START_TIME ", 18) == 0)
            fprintf(f, "REPORT_START_TIME    %02d:%02d:00\n", startMin / 60, startMin % 60);
        else if (strncmp(line, "END_TIME ", 9) == 0)
            fprintf(f, "END_TIME             %02d:%02d:00\n", endMin / 60, endMin % 60);
        else
            fprintf(f, "%.*s\n", (int)(eol - line), line);
        line = *eol ? eol + 1 : eol;
    }
    fclose(f);
}

/* number of reporting periods, from the .out file's closing records */
static int out_periods(void)
{
    int rec[6] = {0, 0, 0, -1, 0, 0};
    FILE *f = fopen(OUT, "rb");
    if (!f) return -1;
    fseek(f, -24L, SEEK_END);
    if (fread(rec, 4, 6, f) != 6) rec[3] = -1;
    fclose(f);
    return rec[5] == 516114522 ? rec[3] : -1;
}

/* run one case; api = 1 sets the end date with swmm_setValueExpanded instead */
static int run_case(int startMin, int endMin, int api, int *periods)
{
    double t = 0.0;
    int err;
    write_case(startMin, api ? 63 : endMin);
    err = swmm_open(CASE, RPT, OUT);
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    if (!err && api)
        err = swmm_setValueExpanded(swmm_SYSTEM, swmm_ENDDATE, 0, 0, 0,
                  swmm_getValueExpanded(swmm_SYSTEM, swmm_STARTDATE, 0, 0, 0)
                  + (endMin - startMin) / 1440.0);
#endif
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    *periods = out_periods();
    return err;
}

int main(void)
{
    static const int show[] = {13, 18, 49, 63, 104, 120};
    int cases[124][2], ncases = 0, nfail[2] = {0, 0}, nruns[2] = {0, 0};
    int i, k, api, nmodes = 1;
    char failed[2][2048] = {"", ""};
    FILE *f = fopen(DECK, "r");
    size_t n = fread(deck, 1, sizeof(deck) - 1, f);
    deck[n] = '\0';
    fclose(f);

    for (i = 1; i <= 120; i++) { cases[ncases][0] = 0; cases[ncases][1] = i; ncases++; }
    cases[ncases][0] = 0;  cases[ncases][1] = 600; ncases++;
    cases[ncases][0] = 60; cases[ncases][1] = 480; ncases++;
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    nmodes = 2;
#endif

    printf("Start  End    Set by      Periods  Expected\n");
    for (api = 0; api < nmodes; api++)
    {
        for (i = 0; i < ncases; i++)
        {
            int s = cases[i][0], e = cases[i][1], periods, err, bad, listed = 0;
            err = run_case(s, e, api, &periods);
            bad = err || periods != e - s;
            nruns[api]++;
            if (bad)
            {
                nfail[api]++;
                if (s == 0)
                    sprintf(failed[api] + strlen(failed[api]), " %02d:%02d", e / 60, e % 60);
                else
                    sprintf(failed[api] + strlen(failed[api]), " %02d:%02d-%02d:%02d",
                            s / 60, s % 60, e / 60, e % 60);
            }
            for (k = 0; k < 6; k++) if (s == 0 && e == show[k]) listed = 1;
            if (e >= 480) listed = 1;
            if (listed)
                printf("%02d:%02d  %02d:%02d  %-10s  %7d  %8d%s\n",
                       s / 60, s % 60, e / 60, e % 60, api ? "setValue" : "input file",
                       periods, e - s,
                       err ? "  <-- error" : (bad ? "  <-- last period missing" : ""));
        }
    }
    for (api = 0; api < nmodes; api++)
        if (nfail[api])
            printf("\n%s: %d of %d runs miss the last period (END_TIME, or START-END):%s\n",
                   api ? "setValue" : "input file", nfail[api], nruns[api], failed[api]);

    if (nfail[0] + nfail[1])
    {
        printf("FAIL: %d of %d runs do not write the last reporting period\n",
               nfail[0] + nfail[1], nruns[0] + nruns[1]);
        return 1;
    }
    printf("PASS: all %d runs write every reporting period up to END_TIME\n",
           nruns[0] + nruns[1]);
    return 0;
}
