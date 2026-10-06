/*
 * IO-35 for 6.0.0: the same check as IO-35_test.c through the 6.0.0 API.
 * 6.0.0 combines a REPORT_START_DATE given alone with a 00:00:00 time, so it
 * is expected to pass. The legacy description follows.
 *
 * Both decks run from 01/01/2020 00:00 to 01/02/2020 06:00 with a 1-hour
 * report step and ask for reporting from 01/02/2020:
 *   IO-35_date-only.inp      REPORT_START_DATE 01/02/2020
 *   IO-35_date-and-time.inp  REPORT_START_DATE 01/02/2020
 *                            REPORT_START_TIME 00:00:00
 * REPORT_START_TIME defaults to 00:00:00 (input reference), so both decks
 * mean the same thing: 7 reporting periods, 01/02 00:00 to 06:00.
 *
 * The test reads each run's .out file: the number of reporting periods and
 * the date stored with the first one (swmm_output_get_period_time). Tolerance
 * on the date: 1 s.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_output.h"

#define JAN2_2020 43832.0      /* 01/02/2020 00:00 in SWMM's day count */

/* runs a deck, then reads its .out file; returns the run's error code */
static int runDeck(const char *inp, const char *rpt, const char *out,
                   int *nPeriods, double *first)
{
    double elapsed = 0.0;
    int err;
    SWMM_Output h;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err) return err;

    h = swmm_output_open(out);
    if (!h) return -1;
    *nPeriods = swmm_output_get_period_count(h);
    if (*nPeriods < 1 || swmm_output_get_period_time(h, 0, first) != 0) err = -1;
    swmm_output_close(h);
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
        snprintf(rpt, sizeof rpt, "IO-35_%s6.rpt", decks[d]);
        snprintf(out, sizeof out, "IO-35_%s6.out", decks[d]);
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
