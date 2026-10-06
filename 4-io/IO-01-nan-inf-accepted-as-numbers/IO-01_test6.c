/*
 * IO-01 for 6.0.0: numeric input fields accept "nan" and "inf".
 *
 * Every numeric token goes through openswmm::from_chars_double()
 * (core/charconv_compat.hpp), i.e. std::from_chars, which reads "nan", "inf"
 * and "infinity" as numbers. The section handlers then store them; checks
 * such as `step <= 0.0` are false for NaN.
 *
 * Correct behaviour, as in IO-01_test.c: a token that is not a finite number
 * is an invalid number (ERROR 211), so swmm_engine_open() fails. The control
 * deck must open and run with every node depth, node volume and link flow
 * finite.
 *
 * For each deck the test prints the open code, the first error from the run,
 * how many non-finite node depths/volumes and link flows it saw, and the
 * routing continuity error.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

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
    printf("%-27s %6s %6s %10s %12s\n", "Deck", "open", "run", "non-finite",
           "routing CE %");
    for (i = 0; i < NDECKS; i++)
    {
        double t = 0.0, v = 0.0, ceFrac = 0.0;
        long nonfinite = 0;
        int openErr, runErr = 0, j, n, bad;
        char ce[32] = "-";
        SWMM_Engine e = swmm_engine_create();

        openErr = swmm_engine_open(e, decks[i], "IO-01_6.rpt", "IO-01_6.out", NULL);
        if (!openErr)
        {
            runErr = swmm_engine_initialize(e);
            if (!runErr) runErr = swmm_engine_start(e, 0);
            while (!runErr)
            {
                runErr = swmm_engine_step(e, &t);
                if (t <= 0.0) break;
                n = swmm_node_count(e);
                for (j = 0; j < n; j++)
                {
                    swmm_node_get_depth(e, j, &v);
                    if (!isfinite(v)) nonfinite++;
                    swmm_node_get_volume(e, j, &v);
                    if (!isfinite(v)) nonfinite++;
                }
                n = swmm_link_count(e);
                for (j = 0; j < n; j++)
                {
                    swmm_link_get_flow(e, j, &v);
                    if (!isfinite(v)) nonfinite++;
                }
            }
            if (!runErr) runErr = swmm_engine_end(e);
            if (!runErr && !swmm_get_routing_continuity_error(e, &ceFrac))
                snprintf(ce, sizeof ce, "%.3f", 100.0 * ceFrac);
        }
        swmm_engine_close(e);
        swmm_engine_destroy(e);

        /* the control deck must open and run cleanly; every other deck must
         * be refused by swmm_engine_open() */
        if (i == 0) bad = openErr != 0 || runErr != 0 || nonfinite != 0;
        else        bad = openErr == 0;
        if (bad) nbad++;
        printf("%-27s %6d %6d %10ld %12s%s\n", decks[i], openErr, runErr, nonfinite,
               ce, bad ? (i == 0 ? "  <-- valid deck failed" : "  <-- accepted") : "");
    }
    if (nbad)
    {
        printf("FAIL: %d of %d decks handled wrongly; 'nan'/'inf' in a numeric field is "
               "accepted as a number instead of raising ERROR 211\n", nbad, NDECKS);
        return 1;
    }
    printf("PASS: 'nan' and 'inf' in numeric fields are refused at swmm_engine_open "
           "(input error), and the finite control deck runs with finite results\n");
    return 0;
}
