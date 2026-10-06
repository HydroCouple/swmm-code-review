/*
 * IO-21: the number of factors in a [PATTERNS] entry is never checked.
 *
 * inflow_readDwfPattern() stores whatever factors a pattern's lines hold,
 * stops reading silently after 24, and inflow_initDwfPattern() has set all
 * 24 to 1.0 beforehand. Nothing compares the count with the 12 / 7 / 24 / 24
 * factors that the input reference requires for MONTHLY / DAILY / HOURLY /
 * WEEKEND patterns, so a pattern with a factor missing uses 1.0 for it and
 * one with a factor too many drops the last one, without a message.
 *
 * Correct behaviour (the documented format): a pattern with the wrong number
 * of factors is an input error, and a complete pattern is applied as given.
 * The test runs a 10 cfs DWF with an HOURLY pattern of 0.5 factors and reads
 * J1's lateral inflow at 23:30, which must be 5.0 cfs when the run is
 * accepted. IO-21_hourly23.inp (23 factors) and IO-21_hourly25.inp (25) must
 * be refused with an error; IO-21_hourly24.inp (24) must run.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static const char *decks[] = {"IO-21_hourly23.inp", "IO-21_hourly24.inp", "IO-21_hourly25.inp"};
static const int nFactors[] = {23, 24, 25};

int main(void)
{
    int i, err, nbad = 0;
    printf("Deck                  Factors  Error code  J1 lateral inflow at 23:30 (cfs)\n");
    for (i = 0; i < 3; i++)
    {
        double t = 0.0, q2330 = -1.0;
        int j1, bad;
        err = swmm_open(decks[i], "IO-21.rpt", "IO-21.out");
        if (!err) err = swmm_start(0);
        j1 = swmm_getIndex(swmm_NODE, "J1");
        while (!err)
        {
            err = swmm_step(&t);
            if (t <= 0.0) break;
            if (q2330 < 0.0 && t * 24.0 >= 23.5) q2330 = swmm_getValue(swmm_NODE_LATFLOW, j1);
        }
        swmm_end();
        swmm_close();
        if (nFactors[i] == 24) bad = err != 0 || fabs(q2330 - 5.0) > 1e-6;
        else bad = err == 0;
        if (bad) nbad++;
        if (q2330 >= 0.0)
            printf("%-20s  %7d  %10d  %10.3f%s\n", decks[i], nFactors[i], err, q2330,
                   bad ? "  <-- accepted without a message" : "");
        else
            printf("%-20s  %7d  %10d  %10s%s\n", decks[i], nFactors[i], err, "(not run)",
                   bad ? "  <-- complete pattern refused" : "");
    }
    if (nbad)
    {
        printf("FAIL: %d of 3 decks handled wrongly; a pattern with a missing or extra factor "
               "runs without a message (missing factor -> 1.0)\n", nbad);
        return 1;
    }
    printf("PASS: patterns with 23 or 25 hourly factors are refused, the complete one runs "
           "as given\n");
    return 0;
}
