/*
 * IO-02 for 6.0.0: clock-time and step options accept nan and huge values.
 *
 * OptionsHandler reads START_TIME, END_TIME, REPORT_START_TIME and the
 * WET/DRY/REPORT/RULE_STEP options with parse_time_seconds(), which takes any
 * number that from_chars() reads (including nan and inf) and applies no range
 * check. A NaN start time and a huge end time go into the run's dates, where
 * datetime::decodeTime() converts them to int; a negative report step is
 * stored as it is.
 *
 * Correct behaviour, as in IO-02_test.c: START_TIME / END_TIME are times of
 * day (0:00:00 to 24:00:00) and steps are positive intervals, so each bad deck
 * is refused by swmm_engine_open(). The control deck must run to its END_TIME
 * of 6:00 (elapsed 0.25 days, to within one 30-s routing step) with a 900 s
 * report step.
 *
 * Every run is capped at 1 simulated day (4 times the intended 6 hours) and
 * 20000 steps, so a run that would not end is stopped and reported.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_model.h"

static const char *decks[] = {
    "IO-02_valid.inp",            /* control                      */
    "IO-02_start-time-nan.inp",   /* START_TIME  nan              */
    "IO-02_end-time-1e30.inp",    /* END_TIME    1e30             */
    "IO-02_report-step-neg.inp",  /* REPORT_STEP -0.25            */
    "IO-02_report-step-huge.inp"  /* REPORT_STEP -1e10            */
};
#define NDECKS 5

int main(void)
{
    int i, nbad = 0;
    printf("%-27s %5s %6s %12s %14s %16s\n", "Deck", "open", "steps", "elapsed (d)",
           "report step s", "start date");
    fflush(stdout);
    for (i = 0; i < NDECKS; i++)
    {
        double t = 0.0, tLast = 0.0, rptStep = 0.0, start = 0.0;
        int openErr, err = 0, steps = 0, capped = 0, bad;
        char buf[64] = "";
        SWMM_Engine e = swmm_engine_create();

        openErr = swmm_engine_open(e, decks[i], "IO-02_6.rpt", "IO-02_6.out", NULL);
        if (!openErr)
        {
            if (!swmm_options_get(e, "REPORT_STEP", buf, sizeof buf)) rptStep = atof(buf);
            swmm_options_get_start_date(e, &start);
            err = swmm_engine_initialize(e);
            if (!err) err = swmm_engine_start(e, 0);
            while (!err)
            {
                err = swmm_engine_step(e, &t);
                if (!(t > 0.0)) break;          /* end of run (or NaN) */
                tLast = t;
                steps++;
                if (t > 1.0 || steps >= 20000) { capped = 1; break; }
            }
            if (!err) swmm_engine_end(e);
        }
        swmm_engine_close(e);
        swmm_engine_destroy(e);

        /* the last non-zero elapsed time is within one 30-s routing step
         * of 0.25 d (the legacy engine returns 0 instead of the final time) */
        if (i == 0) bad = openErr || err || capped || fabs(tLast - 0.25) > 31.0 / 86400.0
                          || rptStep != 900.0;
        else        bad = openErr == 0;
        if (bad) nbad++;
        if (openErr)
            printf("%-27s %5d %6s %12s %14s %16s\n", decks[i], openErr, "-", "-", "-", "-");
        else
            printf("%-27s %5d %6d %12.4f %14.0f %16.4f%s%s\n", decks[i], openErr, steps,
                   tLast, rptStep, start, capped ? "  <-- still running, stopped" : "",
                   bad && !capped ? (i == 0 ? "  <-- control deck wrong" : "  <-- accepted") : "");
        fflush(stdout);
    }
    if (nbad)
    {
        printf("FAIL: %d of %d decks handled wrongly; an invalid START_TIME, END_TIME or "
               "REPORT_STEP is accepted instead of raising an input error\n", nbad, NDECKS);
        return 1;
    }
    printf("PASS: nan, out-of-range and overflowing time options are refused at "
           "swmm_engine_open, and the control deck runs 6 h with a 900 s report step\n");
    return 0;
}
