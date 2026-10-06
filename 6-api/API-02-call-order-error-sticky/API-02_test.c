/*
 * API-02: a call made in the wrong order is stored as the simulation's error.
 *
 * swmm_step/swmm_stride before swmm_start return ERR_API_NOT_STARTED and
 * swmm_start during a run returns ERR_API_NOT_ENDED (5.3.0 adds the same for
 * swmm_saveHotStart before and swmm_useHotStart during a run). Each of them
 * also assigns the code to the global ErrorCode, which swmm_start and
 * swmm_step test first, so a refused call blocks the project until it is
 * closed and opened again, or aborts the run that is going on.
 *
 * Correct behaviour: the out-of-order call is refused (non-zero code) and
 * leaves the project as it was. The run then starts with code 0 and every
 * step returns 0 until the engine signals the end of the 1-hour simulation
 * (elapsed time 0); the last elapsed time reported before that must be
 * within one 30-s routing step of 1 h. Each scenario opens the deck afresh.
 */
#include <stdio.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
#define V530 1          /* 5.3.0: hot start API exists */
#define NCASES 5
#else
#define NCASES 3
#endif

static const char *label[5] = {
    "swmm_step before swmm_start",
    "swmm_stride before swmm_start",
    "swmm_start during the run (step 10)",
    "swmm_saveHotStart before swmm_start",
    "swmm_useHotStart during the run (step 10)"
};

int main(void)
{
    int k, bad = 0;

    printf("%-42s %9s %9s %6s %10s %9s  %s\n", "out-of-order call", "its rc",
           "start rc", "steps", "reached(h)", "last rc", "verdict");
    for (k = 0; k < NCASES; k++)
    {
        double t = 0.0, last = 0.0;
        int misuse = 0, rcStart, rc = 0, steps = 0, ok;

        if (swmm_open("API-02_model.inp", "API-02.rpt", "API-02.out"))
        {
            printf("FAIL: swmm_open failed\n");
            return 1;
        }
        if (k == 0) misuse = swmm_step(&t);
        if (k == 1) misuse = swmm_stride(300, &t);
#ifdef V530
        if (k == 3) misuse = swmm_saveHotStart("API-02.hsf");
#endif
        rcStart = swmm_start(0);
        while (!rc)
        {
            rc = swmm_step(&t);
            if (t <= 0.0) break;
            last = t;
            steps++;
            if (steps == 10 && k == 2) misuse = swmm_start(0);
#ifdef V530
            if (steps == 10 && k == 4) misuse = swmm_useHotStart("API-02.hsf");
#endif
        }
        swmm_end();
        swmm_close();

        /* the run must start (rc 0), no step may fail, and it must get to
           within one 30-s step of 1 h (a run cut short stops by 0.09 h) */
        ok = misuse != 0 && rcStart == 0 && rc == 0 && last * 86400.0 >= 3600.0 - 30.0 - 1e-6;
        if (!ok) bad++;
        printf("%-42s %9d %9d %6d %10.3f %9d  %s\n", label[k], misuse, rcStart,
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
