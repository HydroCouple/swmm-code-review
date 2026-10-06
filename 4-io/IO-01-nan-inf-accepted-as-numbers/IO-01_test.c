/*
 * IO-01: numeric input fields accept "nan" and "inf".
 *
 * getDouble() (input.c) and the decimal-hours branch of datetime_strToTime()
 * (datetime.c) accept any token that strtod() reads completely, and C99
 * strtod() reads "nan", "inf" and "infinity". The range checks that follow are
 * written as x <= 0 or x < 0, which are false for NaN, so a NaN roughness,
 * depth, series value or routing step is stored and the run goes ahead.
 *
 * Correct behaviour: a field that must hold a number rejects a token that is
 * not a finite number with ERROR 211 (invalid number), as it rejects "abc",
 * so swmm_open() fails with the input error code 200. The control deck, with
 * the same network and finite values, must open and run with every node
 * depth, node volume and link flow finite.
 *
 * For each deck the test prints the swmm_open() code, the first swmm_step()
 * error, how many non-finite node depths/volumes and link flows it saw during
 * the run, and the routing continuity error from swmm_getMassBalErr().
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static const char *decks[] = {
    "IO-01_valid.inp",          /* control: all values finite        */
    "IO-01_nan-roughness.inp",  /* [CONDUITS] C1 roughness  nan      */
    "IO-01_inf-depth.inp",      /* [JUNCTIONS] J2 max depth inf      */
    "IO-01_nan-series.inp",     /* [TIMESERIES] TS1 value   nan      */
    "IO-01_nan-routing-step.inp"/* [OPTIONS] ROUTING_STEP   nan      */
};
#define NDECKS 5

int main(void)
{
    int i, nbad = 0;
    printf("%-27s %6s %6s %10s %12s\n", "Deck", "open", "step", "non-finite",
           "routing CE %");
    for (i = 0; i < NDECKS; i++)
    {
        double t = 0.0;
        long nonfinite = 0;
        float runoffErr = 0.0f, flowErr = 0.0f, qualErr = 0.0f;
        int openErr, stepErr = 0, j, n, bad;
        char ce[32] = "-";

        openErr = swmm_open(decks[i], "IO-01.rpt", "IO-01.out");
        if (!openErr)
        {
            stepErr = swmm_start(0);
            while (!stepErr)
            {
                stepErr = swmm_step(&t);
                if (t <= 0.0) break;
                n = swmm_getCount(swmm_NODE);
                for (j = 0; j < n; j++)
                {
                    if (!isfinite(swmm_getValue(swmm_NODE_DEPTH, j))) nonfinite++;
                    if (!isfinite(swmm_getValue(swmm_NODE_VOLUME, j))) nonfinite++;
                }
                n = swmm_getCount(swmm_LINK);
                for (j = 0; j < n; j++)
                    if (!isfinite(swmm_getValue(swmm_LINK_FLOW, j))) nonfinite++;
            }
            swmm_end();
            swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
            snprintf(ce, sizeof ce, "%.3f", flowErr);
        }
        swmm_close();

        /* the control deck must open and run cleanly; every other deck must
         * be refused by swmm_open() */
        if (i == 0) bad = openErr != 0 || stepErr != 0 || nonfinite != 0;
        else        bad = openErr == 0;
        if (bad) nbad++;
        printf("%-27s %6d %6d %10ld %12s%s\n", decks[i], openErr, stepErr, nonfinite,
               ce, bad ? (i == 0 ? "  <-- valid deck failed" : "  <-- accepted") : "");
    }
    if (nbad)
    {
        printf("FAIL: %d of %d decks handled wrongly; 'nan'/'inf' in a numeric field is "
               "accepted as a number instead of raising ERROR 211\n", nbad, NDECKS);
        return 1;
    }
    printf("PASS: 'nan' and 'inf' in numeric fields are refused at swmm_open (input error), "
           "and the finite control deck runs with finite results\n");
    return 0;
}
