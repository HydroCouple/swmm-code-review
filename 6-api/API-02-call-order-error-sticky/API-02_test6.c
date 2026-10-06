/*
 * API-02 for 6.0.0: a call made in the wrong order puts the engine in its
 * error state.
 *
 * SWMMEngine's lifecycle functions refuse a call made in the wrong state with
 * SWMM_ERR_LIFECYCLE (6), but they report it through set_error(), which also
 * moves the engine to ERROR_STATE. From there swmm_engine_start refuses to
 * start (it wants INITIALIZED) and swmm_engine_step refuses to step (it wants
 * RUNNING), so the refused call blocks the project or aborts the running
 * simulation.
 *
 * Correct behaviour: the out-of-order call is refused (non-zero code) and
 * leaves the engine as it was. The run then starts with code 0 and every
 * step returns 0 until the engine signals the end of the 1-hour simulation
 * (elapsed time 0); the last elapsed time reported before that must be
 * within one 30-s routing step of 1 h. Each scenario opens the deck afresh.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"

#define NCASES 7

static const char *label[NCASES] = {
    "swmm_engine_step before start",
    "swmm_engine_stride before start",
    "swmm_engine_end before start",
    "swmm_engine_start during the run (step 10)",
    "swmm_engine_initialize during the run",
    "swmm_engine_report during the run",
    "swmm_engine_open during the run"
};

int main(void)
{
    int k, bad = 0;

    printf("%-44s %7s %9s %6s %10s %8s  %s\n", "out-of-order call", "its rc",
           "start rc", "steps", "reached(h)", "last rc", "verdict");
    for (k = 0; k < NCASES; k++)
    {
        SWMM_Engine e = swmm_engine_create();
        double t = 0.0, last = 0.0;
        int misuse = 0, rcStart, rc = 0, steps = 0, ok;

        rc = swmm_engine_open(e, "API-02_model.inp", "API-02_6.rpt", "API-02_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (rc)
        {
            printf("FAIL: could not open the deck (error %d)\n", rc);
            return 1;
        }
        if (k == 0) misuse = swmm_engine_step(e, &t);
        if (k == 1) misuse = swmm_engine_stride(e, 10, &t);
        if (k == 2) misuse = swmm_engine_end(e);
        rcStart = swmm_engine_start(e, 0);
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
            last = t;
            steps++;
            if (steps != 10) continue;
            if (k == 3) misuse = swmm_engine_start(e, 0);
            if (k == 4) misuse = swmm_engine_initialize(e);
            if (k == 5) misuse = swmm_engine_report(e);
            if (k == 6) misuse = swmm_engine_open(e, "API-02_model.inp", "x.rpt", "x.out", NULL);
        }
        swmm_engine_end(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);

        /* the run must start (rc 0), no step may fail, and it must get to
           within one 30-s step of 1 h (a run cut short stops by 0.09 h) */
        ok = misuse != 0 && rcStart == 0 && rc == 0 && last * 86400.0 >= 3600.0 - 30.0 - 1e-6;
        if (!ok) bad++;
        printf("%-44s %7d %9d %6d %10.3f %8d  %s\n", label[k], misuse, rcStart,
               steps, last * 24.0, rc, ok ? "ok" : "PROJECT BLOCKED");
    }

    if (bad)
    {
        printf("FAIL: in %d of %d cases a refused out-of-order call stopped the "
               "run from starting or finishing\n", bad, NCASES);
        return 1;
    }
    printf("PASS: every out-of-order call is refused and the run still starts "
           "and reaches the end\n");
    return 0;
}
