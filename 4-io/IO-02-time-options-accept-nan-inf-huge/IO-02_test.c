/*
 * IO-02: clock-time and step options accept nan, inf and huge values.
 *
 * START_TIME, END_TIME, REPORT_START_TIME and the WET/DRY/REPORT/RULE_STEP
 * options are read by datetime_strToTime(), which takes a bare number as
 * decimal hours with no range check. project_readOption() stores the times
 * as they are, and turns a step into seconds with int arithmetic
 * (h += 24*(int)aTime; s = s + 60*m + 3600*h), which overflows above
 * 596,523 hours. A negative step of -1e10 hours wraps to a positive number
 * of seconds and passes the "s <= 0" test.
 *
 * Correct behaviour (input reference: START_TIME / END_TIME are times of day,
 * HH:MM:SS, so 0:00:00 to 24:00:00; steps are positive intervals): each bad
 * deck is refused by swmm_open() with an input error. The control deck must
 * run to its END_TIME of 6:00 (elapsed 0.25 days, to within one 30-s routing
 * step) with a 900 s report step.
 *
 * Every run is capped at 1 simulated day (4 times the intended 6 hours) and
 * 20000 steps, so a run that would not end is stopped and reported.
 * The REPORT_STEP -1e10 deck is last: in a sanitizer build the int overflow
 * in project_readOption() stops the program there.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static const char *decks[] = {
    "IO-02_valid.inp",            /* control                      */
    "IO-02_start-time-nan.inp",   /* START_TIME  nan              */
    "IO-02_end-time-1e30.inp",    /* END_TIME    1e30 (hours)     */
    "IO-02_report-step-neg.inp",  /* REPORT_STEP -0.25 (hours)    */
    "IO-02_report-step-huge.inp"  /* REPORT_STEP -1e10 (hours)    */
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

        openErr = swmm_open(decks[i], "IO-02.rpt", "IO-02.out");
        if (!openErr)
        {
            err = swmm_start(0);
            rptStep = swmm_getValue(swmm_REPORTSTEP, 0);
            start = swmm_getValue(swmm_STARTDATE, 0);
            while (!err)
            {
                err = swmm_step(&t);
                if (!(t > 0.0)) break;          /* end of run (or NaN) */
                tLast = t;
                steps++;
                if (t > 1.0 || steps >= 20000) { capped = 1; break; }
            }
            swmm_end();
        }
        swmm_close();

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
    printf("PASS: nan, out-of-range and overflowing time options are refused at swmm_open, "
           "and the control deck runs 6 h with a 900 s report step\n");
    return 0;
}
