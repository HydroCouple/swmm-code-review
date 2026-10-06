/*
 * BND-09 for 6.0.0: the run stops 1 s early and loses its last reporting period.
 *
 * 6.0.0 copies 5.3.0's formula for parity (OptionsHandler.cpp):
 *   total_duration_ms = floor((end_date_part - start_date_part) * 86400 +
 *                             (end_time_part - start_time_part) * 86400) * 1000
 * and, when the dates are set through the API, SimulationOptions::totalDurationMs()
 * falls back to 5.2.4's floor((end_date - start_date) * 86400) * 1000.
 * For many start/end times the product is 1 ulp below the whole number of
 * seconds and floor() drops a whole second.
 *
 * Correct behaviour: a run from START to END with REPORT_STEP 1 min writes
 * (END - START) / 1 min reporting periods to the .out file and its last
 * routing step ends at END. Same sweep as BND-09_test.c: every whole minute
 * from 00:01 to 02:00 after a 00:00 start plus 00:00-10:00 and 01:00-08:00,
 * once from the input file and once with the end date set through
 * swmm_options_set_end_date(). The bug is a whole second and a whole period;
 * the test allows 0.5 s.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_model.h"

#define DECK  "BND-09_end0103.inp"
#define CASE  "BND-09_case6.inp"
#define RPT   "BND-09_case6.rpt"
#define OUT   "BND-09_case6.out"

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

/* run one case; api = 1 sets the end date with swmm_options_set_end_date */
static int run_case(int startMin, int endMin, int api, int *periods, double *lastSec)
{
    double t = 0.0, last = 0.0, start = 0.0;
    int rc;
    SWMM_Engine e = swmm_engine_create();
    write_case(startMin, api ? 63 : endMin);
    rc = swmm_engine_open(e, CASE, RPT, OUT, NULL);
    if (!rc && api)
    {
        rc = swmm_options_get_start_date(e, &start);
        if (!rc) rc = swmm_options_set_end_date(e, start + (endMin - startMin) / 1440.0);
    }
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        last = t;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    *periods = out_periods();
    *lastSec = last * 86400.0;
    return rc;
}

int main(void)
{
    static const int show[] = {13, 18, 49, 63, 104, 120};
    int cases[124][2], ncases = 0, nfail[2] = {0, 0}, nruns[2] = {0, 0};
    int i, k, api;
    char failed[2][2048] = {"", ""};
    FILE *f = fopen(DECK, "r");
    size_t n = fread(deck, 1, sizeof(deck) - 1, f);
    deck[n] = '\0';
    fclose(f);

    for (i = 1; i <= 120; i++) { cases[ncases][0] = 0; cases[ncases][1] = i; ncases++; }
    cases[ncases][0] = 0;  cases[ncases][1] = 600; ncases++;
    cases[ncases][0] = 60; cases[ncases][1] = 480; ncases++;

    printf("Start  End    Set by      Periods  Expected  Last step ends (s)  Expected (s)\n");
    for (api = 0; api < 2; api++)
    {
        for (i = 0; i < ncases; i++)
        {
            int s = cases[i][0], e = cases[i][1], periods, err, bad, listed = 0;
            double last, expSec = 60.0 * (e - s);
            err = run_case(s, e, api, &periods, &last);
            bad = err || periods != e - s || fabs(last - expSec) > 0.5;
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
                printf("%02d:%02d  %02d:%02d  %-10s  %7d  %8d  %18.1f  %12.1f%s\n",
                       s / 60, s % 60, e / 60, e % 60, api ? "API" : "input file",
                       periods, e - s, last, expSec,
                       err ? "  <-- error" : (bad ? "  <-- last period missing" : ""));
        }
    }
    for (api = 0; api < 2; api++)
        if (nfail[api])
            printf("\n%s: %d of %d runs end 1 s early (END_TIME, or START-END):%s\n",
                   api ? "API" : "input file", nfail[api], nruns[api], failed[api]);

    if (nfail[0] + nfail[1])
    {
        printf("FAIL: %d of %d runs stop 1 s before END_TIME and do not write the last "
               "reporting period\n", nfail[0] + nfail[1], nruns[0] + nruns[1]);
        return 1;
    }
    printf("PASS: all %d runs reach END_TIME and write every reporting period\n",
           nruns[0] + nruns[1]);
    return 0;
}
