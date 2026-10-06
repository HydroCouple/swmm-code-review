/*
 * CRASH-13 for 6.0.0: pollutant mass flux set before the run starts.
 *
 * 6.0.0 has no link pollutant flux. Its node flux setters,
 * swmm_node_set_quality_mass_flux() and swmm_forcing_node_quality(), are
 * called between swmm_engine_initialize and swmm_engine_start with a valid
 * and an out-of-range pollutant index. Correct, as in the legacy test: no
 * crash, and each call is either refused with a non-zero code or used by the
 * run (J1's P1 concentration becomes > 0; the deck has no other P1 source).
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_forcing.h"

int main(void)
{
    const char *label[4] = {
        "swmm_node_set_quality_mass_flux(J1, P1 = 0)",
        "swmm_node_set_quality_mass_flux(J1, pollutant 2)",
        "swmm_forcing_node_quality(J1, P1 = 0)",
        "swmm_forcing_node_quality(J1, pollutant 2)" };
    int pol[4] = { 0, 2, 0, 2 };
    int i, bad = 0;

    printf("Flux 5.0 set before swmm_engine_start (2 pollutants)\n");
    printf("%-50s %4s   %10s   %s\n", "call", "rc", "max conc", "result");
    for (i = 0; i < 4; i++)
    {
        SWMM_Engine e = swmm_engine_create();
        double t = 0.0, c = 0.0, cMax = 0.0;
        int j1, set, rc = swmm_engine_open(e, "CRASH-13_two-pollutants.inp",
                                           "CRASH-13_6.rpt", "CRASH-13_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        j1 = swmm_node_index(e, "J1");
        if (i < 2) set = swmm_node_set_quality_mass_flux(e, j1, pol[i], 5.0);
        else       set = swmm_forcing_node_quality(e, j1, pol[i], 5.0,
                                                   SWMM_FORCING_ADD, SWMM_FORCING_PERSIST);
        if (!rc) rc = swmm_engine_start(e, 1);
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0) break;
            if (swmm_node_get_quality(e, j1, 0, &c) == 0 && c > cMax) cMax = c;
        }
        if (!rc) rc = swmm_engine_end(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);

        printf("%-50s %4d   %10.4f   %s\n", label[i], set, cMax,
               set != 0 ? "refused" : (cMax > 0.0 ? "used by the run" : "ACCEPTED BUT IGNORED"));
        if (rc) printf("  run error code %d\n", rc);
        if (rc || (set == 0 && cMax <= 0.0)) bad++;
    }
    if (bad)
    {
        printf("FAIL: %d of 4 pre-start flux calls were accepted and ignored (or the run failed)\n",
               bad);
        return 1;
    }
    printf("PASS: every pre-start pollutant flux call is refused with an error code or used "
           "by the run, without a crash\n");
    return 0;
}
