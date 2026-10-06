/*
 * CON-06 for 6.0.0: snow plowed to another subcatchment is scaled by area
 * fractions only (SnowSolver::plowSnow(), Snow.cpp, as legacy snow.c).
 *
 * Same decks and check as CON-06_test.c: 1 in of snow on S1 (10 ac, all
 * plowable) and S2, everything S1 collects is plowed onto S2, no losses, all
 * snow melts within the run. The runoff continuity error must stay within 1%
 * (the bug gives about -80% with S2 = 100 ac and +80% with S2 = 1 ac).
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

#define FT3_PER_ACFT 43560.0

static int run(const char *inp, const char *rpt, const char *out, double v[5])
{
    double t = 0.0;
    int i, rc;
    const int comp[4] = {SWMM_RUNOFF_RAINFALL, SWMM_RUNOFF_RUNOFF,
                         SWMM_RUNOFF_FINALSNOW, SWMM_RUNOFF_FINALSTORE};
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    for (i = 0; i < 4 && !rc; i++)
    {
        rc = swmm_get_runoff_total(e, comp[i], &v[i]);
        v[i] /= FT3_PER_ACFT;
    }
    if (!rc) rc = swmm_get_runoff_continuity_error(e, &v[4]);
    v[4] *= 100.0;
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    const char *name[2] = {"S2 = 100 ac", "S2 = 1 ac"};
    const char *inp[2] = {"CON-06_to-larger.inp", "CON-06_to-smaller.inp"};
    const char *rpt[2] = {"CON-06_to-larger6.rpt", "CON-06_to-smaller6.rpt"};
    const char *out[2] = {"CON-06_to-larger6.out", "CON-06_to-smaller6.out"};
    double v[2][5];
    int i, rc, ok = 1;

    printf("Snow plowed from S1 (10 ac) to S2      volumes in acre-feet\n");
    printf("Case          Precipitation  Runoff   Final snow  Final storage  Continuity error\n");
    for (i = 0; i < 2; i++)
    {
        rc = run(inp[i], rpt[i], out[i], v[i]);
        if (rc)
        {
            printf("FAIL: %s stopped with error %d\n", inp[i], rc);
            return 1;
        }
        printf("%-12s  %10.3f    %7.3f   %8.3f     %8.3f        %8.3f %%\n",
               name[i], v[i][0], v[i][1], v[i][2], v[i][3], v[i][4]);
        /* the fixed code gives about -0.1%; the bug about +-80% */
        if (v[i][4] > 1.0 || v[i][4] < -1.0) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: plowing snow to another subcatchment changes its volume: "
               "runoff continuity errors %.3f%% and %.3f%%\n", v[0][4], v[1][4]);
        return 1;
    }
    printf("PASS: plowed snow keeps its volume (runoff continuity errors "
           "%.3f%% and %.3f%%)\n", v[0][4], v[1][4]);
    return 0;
}
