/*
 * CRASH-06 for 6.0.0: the same deck through the 6.0.0 C API. 6.0.0 parses
 * SWEEP_START with std::from_chars on a std::string (OptionsHandler.cpp) and
 * has no fixed-size buffer, so it is not affected. The test passes when
 * opening and running the deck returns, error or not, and prints the codes.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"

int main(void)
{
    double t = 0.0;
    int rc, rc_open;
    SWMM_Engine e = swmm_engine_create();

    rc = rc_open = swmm_engine_open(e, "CRASH-06_long-sweep-date.inp",
                                    "CRASH-06_6.rpt", "CRASH-06_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    printf("swmm_engine_open() returned %d, the run returned %d\n", rc_open, rc);
    printf("PASS: the 25-character SWEEP_START value is handled without a "
           "memory error (code %d)\n", rc);
    return 0;
}
