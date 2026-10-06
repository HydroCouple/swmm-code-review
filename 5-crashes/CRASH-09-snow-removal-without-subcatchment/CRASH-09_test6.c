/*
 * CRASH-09 for 6.0.0: [SNOWPACKS] REMOVAL with Fsub > 0 but no receiving
 * subcatchment. Legacy snow_plowSnow() indexes Subcatch[-1]; 6.0.0's
 * SnowSolver::plowSnow() skips the transfer when to_subcatch < 0, so the
 * snow stays on the donor's plowable area.
 *
 * Same deck and check as CRASH-09_test.c: the run must either reject the
 * input with an error code, or complete with a runoff continuity error
 * within 1% (all snow melts and runs off; about -0.6% from the time stepping).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

#define FT3_PER_IN_10AC (10.0 * 43560.0 / 12.0)

int main(void)
{
    double t = 0.0, init = 0.0, runoff = 0.0, fin = 0.0, cerr = 0.0;
    int rc, steps = 0;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "CRASH-09_fsub-no-subcatch.inp", "CRASH-09_6.rpt",
                          "CRASH-09_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (rc)
    {
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        printf("PASS: the input is rejected with error %d\n", rc);
        return 0;
    }
    rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        steps++;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_get_runoff_total(e, SWMM_RUNOFF_INITSNOW, &init);
    if (!rc) rc = swmm_get_runoff_total(e, SWMM_RUNOFF_RUNOFF, &runoff);
    if (!rc) rc = swmm_get_runoff_total(e, SWMM_RUNOFF_FINALSNOW, &fin);
    if (!rc) rc = swmm_get_runoff_continuity_error(e, &cerr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d after %d steps\n", rc, steps);
        return 1;
    }
    cerr *= 100.0;

    printf("Runoff steps run: %d\n", steps);
    printf("Initial snow cover  %6.3f in\n", init / FT3_PER_IN_10AC);
    printf("Surface runoff      %6.3f in\n", runoff / FT3_PER_IN_10AC);
    printf("Final snow cover    %6.3f in\n", fin / FT3_PER_IN_10AC);
    printf("Runoff continuity error %.3f %%\n", cerr);

    /* the run with the snow kept gives about -0.6%; 1% leaves margin */
    if (fabs(cerr) > 1.0)
    {
        printf("FAIL: the run completed but lost or created snow: runoff "
               "continuity error %.3f%%\n", cerr);
        return 1;
    }
    printf("PASS: the run completed, the snow with no receiving subcatchment "
           "stayed on S1 (runoff continuity error %.3f%%)\n", cerr);
    return 0;
}
