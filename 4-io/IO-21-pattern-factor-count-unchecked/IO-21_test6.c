/*
 * IO-21 for 6.0.0: the number of factors in a [PATTERNS] entry is never
 * checked (InflowsHandler.cpp handle_patterns() appends whatever factors the
 * lines hold, and getPatternFactor() uses 1.0 for a missing one).
 *
 * Correct behaviour, as in IO-21_test.c: a pattern with the wrong number of
 * factors is an input error, and a complete pattern is applied as given
 * (10 cfs DWF x 0.5 = 5.0 cfs at 23:30). IO-21_hourly23.inp (23 factors)
 * and IO-21_hourly25.inp (25) must be refused; IO-21_hourly24.inp must run.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

static const char *decks[] = {"IO-21_hourly23.inp", "IO-21_hourly24.inp", "IO-21_hourly25.inp"};
static const int nFactors[] = {23, 24, 25};

int main(void)
{
    int i, rc, nbad = 0;
    printf("Deck                  Factors  Error code  J1 lateral inflow at 23:30 (cfs)\n");
    for (i = 0; i < 3; i++)
    {
        double t = 0.0, q = 0.0, q2330 = -1.0;
        int j1 = -1, bad;
        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i], "IO-21_6.rpt", "IO-21_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 0);
        if (!rc) j1 = swmm_node_index(e, "J1");
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
            if (q2330 < 0.0 && t * 24.0 >= 23.5)
            {
                swmm_node_get_lateral_inflow(e, j1, &q);
                q2330 = q;
            }
        }
        if (!rc) rc = swmm_engine_end(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        if (nFactors[i] == 24) bad = rc != 0 || fabs(q2330 - 5.0) > 1e-6;
        else bad = rc == 0;
        if (bad) nbad++;
        if (q2330 >= 0.0)
            printf("%-20s  %7d  %10d  %10.3f%s\n", decks[i], nFactors[i], rc, q2330,
                   bad ? "  <-- accepted without a message" : "");
        else
            printf("%-20s  %7d  %10d  %10s%s\n", decks[i], nFactors[i], rc, "(not run)",
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
