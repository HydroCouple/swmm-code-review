/*
 * CON-12 for 6.0.0: a cyclic dependency in treatment functions must be
 * reported as ERROR 161.
 *
 * 6.0.0 checks the R_<pollutant> dependency graph of each node when it loads
 * the model (SWMMEngine::initQuality) and reports ERROR 161 for a cycle, but
 * it leaves out a pollutant's reference to its own removal, so the shortest
 * cycle (TN  R = 0.5*R_TN) runs. At run time it copies legacy's silent latch.
 *
 * Correct behaviour, as in CON-12_test.c: both decks stop with ERROR 161.
 * 6.0.0 returns its own error code (SWMM_ERR_PARSE) and puts the legacy
 * message, "ERROR 161: cyclic dependency ...", in swmm_get_last_error_msg().
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

static const char *decks[] = {"CON-12_self-reference.inp", "CON-12_mutual.inp"};

int main(void)
{
    int i, rc, nbad = 0;

    printf("Deck                         Error code  Message\n");
    for (i = 0; i < 2; i++)
    {
        double t = 0.0;
        const char *msg;
        int ok;
        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i], "CON-12_6.rpt", "CON-12_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
        }
        if (!rc) rc = swmm_engine_end(e);
        if (!rc) rc = swmm_engine_report(e);
        msg = swmm_get_last_error_msg(e);
        ok = rc != 0 && msg != NULL && strstr(msg, "161") != NULL;
        if (!ok) nbad++;
        printf("%-28s %10d  %s%s\n", decks[i], rc, rc && msg ? msg : "(run completed)",
               ok ? "" : "  <-- no ERROR 161");
        swmm_engine_close(e);
        swmm_engine_destroy(e);
    }
    if (nbad)
    {
        printf("FAIL: %d of 2 decks with a cyclic treatment dependency run without ERROR 161\n",
               nbad);
        return 1;
    }
    printf("PASS: both cyclic treatment dependencies stop the run with ERROR 161\n");
    return 0;
}
