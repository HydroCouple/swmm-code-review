/*
 * IO-49: the .out header's "report start date" is wrong when REPORT_START is
 * later than START.
 *
 * The binary output file stores each period's date, but the reader API
 * (SMO_getStartDate + SMO_getTimes(SMO_reportStep)) and the EPA GUI build the
 * time axis from the header: period k (0-based) is at
 *     header date + (k + 1) * REPORT_STEP
 * output_open() promises exactly that ("make saved starting report date one
 * reporting period prior to the date of the first reported result") but
 * computes floor((ReportStart - StartDateTime) / step) - 1 steps. The first
 * saved period is the first report time at or after REPORT_START, which is a
 * ceil(), and for an aligned REPORT_START the floating-point quotient is often
 * n - 2e-10, so floor() is one short there as well.
 *
 * Correct behaviour: header date + REPORT_STEP equals the stored date of the
 * first period (to 1 s: stored dates carry SWMM's +1 ms offset; the bug is a
 * whole report step of 60 s or more). The test rewrites REPORT_START_TIME and
 * REPORT_STEP of IO-49_report-start.inp (START 00:00, END 03:00) for report
 * steps of 1, 5, 15 and 60 min and every whole-minute report start from 00:01
 * to 02:00, runs each case and reads the header and first period from the .out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define DECK "IO-49_report-start.inp"
#define CASE "IO-49_case.inp"
#define RPT  "IO-49_case.rpt"
#define OUT  "IO-49_case.out"

static char deck[8192];

/* write CASE: DECK with REPORT_START_TIME and REPORT_STEP replaced */
static void write_case(int rsMin, int stepSec)
{
    FILE *f = fopen(CASE, "w");
    char *line = deck, *eol;
    while (*line)
    {
        eol = strchr(line, '\n');
        if (!eol) eol = line + strlen(line);
        if (strncmp(line, "REPORT_START_TIME ", 18) == 0)
            fprintf(f, "REPORT_START_TIME    %02d:%02d:00\n", rsMin / 60, rsMin % 60);
        else if (strncmp(line, "REPORT_STEP ", 12) == 0)
            fprintf(f, "REPORT_STEP          %02d:%02d:%02d\n",
                    stepSec / 3600, (stepSec / 60) % 60, stepSec % 60);
        else
            fprintf(f, "%.*s\n", (int)(eol - line), line);
        line = *eol ? eol + 1 : eol;
    }
    fclose(f);
}

/* header date, report step and first stored period date from the .out file */
static int read_out(double *header, int *step, double *first)
{
    int rec[6];
    FILE *f = fopen(OUT, "rb");
    if (!f) return 0;
    fseek(f, -24L, SEEK_END);
    if (fread(rec, 4, 6, f) != 6 || rec[5] != 516114522 || rec[3] < 1)
    {
        fclose(f);
        return 0;
    }
    fseek(f, rec[2] - 12L, SEEK_SET);           /* REAL8 date, INT4 step */
    if (fread(header, 8, 1, f) != 1 || fread(step, 4, 1, f) != 1 ||
        fread(first, 8, 1, f) != 1) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

/* "hh:mm" for a number of seconds after START (negative: previous day) */
static const char *hhmm(double secs)
{
    static char buf[4][16];
    static int slot = 0;
    int m = (int)floor(secs / 60.0 + 0.5);
    char *b = buf[slot++ % 4];
    if (m < 0) sprintf(b, "%02d:%02d-1d", (m + 1440) / 60, (m + 1440) % 60);
    else       sprintf(b, "%02d:%02d   ", m / 60, m % 60);
    return b;
}

static int run_case(void)
{
    double t = 0.0;
    int err = swmm_open(CASE, RPT, OUT);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    return err;
}

int main(void)
{
    static const int steps[] = {60, 300, 900, 3600};
    int i, k, nruns = 0, nbad[4] = {0, 0, 0, 0}, nerr = 0;
    FILE *f = fopen(DECK, "r");
    size_t n = fread(deck, 1, sizeof(deck) - 1, f);
    deck[n] = '\0';
    fclose(f);

    printf("Step  Rpt start  First period  Header      Should be   Error (steps)\n");
    for (k = 0; k < 4; k++)
    {
        for (i = 1; i <= 120; i++)
        {
            double header = 0, first = 0, h, need;
            int step = 0, err, bad, show;
            write_case(i, steps[k]);
            err = run_case();
            nruns++;
            if (err || !read_out(&header, &step, &first))
            {
                printf("%3dm  %02d:%02d      run error %d\n", steps[k] / 60, i / 60, i % 60, err);
                nerr++;
                continue;
            }
            /* seconds after START (01/01/2020 00:00 = day 43831) */
            h = (header - 43831.0) * 86400.0;
            need = (first - 43831.0) * 86400.0 - step;
            bad = fabs(h - need) > 1.0;
            if (bad) nbad[k]++;
            show = (i == 1 || i == 2 || i == 20 || i == 60 || i == 75 || i == 90);
            if (show)
                printf("%3dm  %02d:%02d      %s      %s  %s  %+d\n",
                       steps[k] / 60, i / 60, i % 60, hhmm(need + step), hhmm(h),
                       hhmm(need), (int)floor((h - need) / step + 0.5));
        }
    }
    printf("\nRuns with a wrong header date (of 120 report starts per step):\n");
    for (k = 0; k < 4; k++) printf("  REPORT_STEP %2d min: %d\n", steps[k] / 60, nbad[k]);

    if (nerr || nbad[0] + nbad[1] + nbad[2] + nbad[3])
    {
        printf("FAIL: in %d of %d runs header date + REPORT_STEP is not the first saved "
               "period's date%s\n", nbad[0] + nbad[1] + nbad[2] + nbad[3], nruns,
               nerr ? " (and some runs stopped with an error)" : "");
        return 1;
    }
    printf("PASS: in all %d runs header date + REPORT_STEP is the first saved period's date\n",
           nruns);
    return 0;
}
