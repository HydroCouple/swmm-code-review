/*
 * IO-35: REPORT_START_DATE without REPORT_START_TIME is ignored.
 *
 * Both decks run from 01/01/2020 00:00 to 01/02/2020 06:00 with a 1-hour
 * report step and ask for reporting from 01/02/2020:
 *   IO-35_date-only.inp      REPORT_START_DATE 01/02/2020
 *   IO-35_date-and-time.inp  REPORT_START_DATE 01/02/2020
 *                            REPORT_START_TIME 00:00:00
 * REPORT_START_TIME defaults to 00:00:00 (input reference), so both decks
 * mean the same thing: 7 reporting periods, 01/02 00:00 to 06:00.
 *
 * setDefaults() initialises ReportStartTime to NO_DATE (-693594 days, i.e.
 * 1 Jan 0001), so with the date alone ReportStart = date - 693594 days, and
 * MAX(ReportStart, StartDateTime) quietly moves it to the simulation start.
 *
 * The test reads each run's .out file: the number of reporting periods and
 * the date of the first one (the .out header holds the date one report step
 * before the first saved period). Tolerance on the date: 1 s.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

#define JAN2_2020 43832.0      /* 01/02/2020 00:00 in SWMM's day count */

/* runs a deck, then reads its .out file; returns the run's error code */
static int runDeck(const char *inp, const char *rpt, const char *out,
                   int *nPeriods, double *first)
{
    double elapsed = 0.0, start = 0.0;
    int err, step = 0;
    SMO_Handle h = NULL;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err) return err;

    SMO_init(&h);
    if (SMO_open(h, out) != 0 ||
        SMO_getTimes(h, SMO_numPeriods, nPeriods) != 0 ||
        SMO_getTimes(h, SMO_reportStep, &step) != 0 ||
        SMO_getStartDate(h, &start) != 0) err = -1;
    SMO_close(&h);
    *first = start + step / 86400.0;
    return err;
}

/* prints a SWMM date as MM/DD/YYYY HH:MM */
static void printDate(double d)
{
    /* day 43831 = 01/01/2020; the decks stay within 01/01-01/02/2020 */
    int day = (int)floor(d + 1.0e-6) - 43831 + 1;
    int mins = (int)floor((d - floor(d + 1.0e-6)) * 1440.0 + 0.5);
    if (mins < 0) mins = 0;
    printf("01/%02d/2020 %02d:%02d", day, mins / 60, mins % 60);
}

int main(void)
{
    const char *decks[2] = {"date-only", "date-and-time"};
    char inp[64], rpt[64], out[64];
    int d, err[2], n[2] = {0, 0}, ok = 1;
    double first[2] = {0.0, 0.0};

    for (d = 0; d < 2; d++)
    {
        snprintf(inp, sizeof inp, "IO-35_%s.inp", decks[d]);
        snprintf(rpt, sizeof rpt, "IO-35_%s.rpt", decks[d]);
        snprintf(out, sizeof out, "IO-35_%s.out", decks[d]);
        err[d] = runDeck(inp, rpt, out, &n[d], &first[d]);
    }

    printf("Reporting periods in the .out file (asked for: from 01/02/2020):\n");
    printf("  %-14s %6s %8s   %s\n", "deck", "error", "periods", "first period");
    for (d = 0; d < 2; d++)
    {
        printf("  %-14s %6d %8d   ", decks[d], err[d], n[d]);
        printDate(first[d]);
        printf("\n");
        if (err[d] || n[d] != 7 || fabs(first[d] - JAN2_2020) > 1.0 / 86400.0) ok = 0;
    }
    printf("  (correct: 7 periods, the first at 01/02/2020 00:00, for both decks)\n");

    if (!ok)
    {
        d = (err[0] || n[0] != 7 || fabs(first[0] - JAN2_2020) > 1.0 / 86400.0) ? 0 : 1;
        if (err[d]) printf("FAIL: %s stopped with error %d\n", decks[d], err[d]);
        else
        {
            printf("FAIL: %s reports %d periods from ", decks[d], n[d]);
            printDate(first[d]);
            printf(" instead of 7 from 01/02/2020 00:00\n");
        }
        return 1;
    }
    printf("PASS: reporting starts on REPORT_START_DATE whether or not REPORT_START_TIME is given\n");
    return 0;
}
