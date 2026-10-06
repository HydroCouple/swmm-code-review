/*
 * CON-23 for 6.0.0: a rain gage that only a control rule reads is never
 * updated, so its INTENSITY premise does not follow the gage's record.
 *
 * Same decks and checks as CON-23_test.c. updateAllGages() advances only the
 * gages that gageIsUsed() reports (a subcatchment or unit hydrograph names
 * them); any other gage keeps the rainfall of its first record.
 *
 * Gage G2 records 0.5 in/hr from 01:00 to 01:15. No subcatchment uses it;
 * rule GR reads it:
 *     IF GAGE G2 INTENSITY > 0.1 THEN ORIFICE OR1 SETTING = 0.2
 *     ELSE ORIFICE OR1 SETTING = 1.0
 * CON-23_no-subcatch.inp has no subcatchments at all; CON-23_with-subcatch.inp
 * has one, on another (dry) gage G1, so runoff is computed. In both decks the
 * [CONTROLS] section comes before [RAINGAGES].
 *
 * Correct behaviour: the premise sees G2's recorded intensity, so OR1 is 0.2
 * while it rains and 1.0 otherwise. Sampled at 0:30, 1:07 and 2:00 the setting
 * must be 1.0, 0.2, 1.0, and the time OR1 spends at 0.2 must be 15 min (to
 * within one 30 s routing step). With the bug the premise reads 0 and OR1
 * stays at 1.0 for the whole run.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

static int runDeck(const char *inp, const char *rpt, const char *out,
                   double set[3], double *minutesThrottled)
{
    double t = 0.0, tPrev = 0.0, s = 0.0, sample[3] = {0.5, 7.0 / 60.0 + 1.0, 2.0}; /* hours */
    int rc, L = -1, k = 0;
    SWMM_Engine e = swmm_engine_create();

    *minutesThrottled = 0.0;
    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    if (!rc) L = swmm_link_index(e, "OR1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_link_get_control_setting(e, L, &s);
        if (fabs(s - 0.2) < 1e-9) *minutesThrottled += (t - tPrev) * 1440.0;
        tPrev = t;
        if (k < 3 && t * 24.0 >= sample[k] - 1e-9) set[k++] = s;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (!rc && k < 3) rc = -1;
    return rc;
}

int main(void)
{
    const char *name[2] = {"no subcatchments", "subcatchment on G1"};
    double set[2][3], minutes[2], expSet[3] = {1.0, 0.2, 1.0};
    int err[2], d, i, ok = 1;

    err[0] = runDeck("CON-23_no-subcatch.inp", "CON-23_a6.rpt", "CON-23_a6.out", set[0], &minutes[0]);
    err[1] = runDeck("CON-23_with-subcatch.inp", "CON-23_b6.rpt", "CON-23_b6.out", set[1], &minutes[1]);
    if (err[0] || err[1])
    {
        printf("FAIL: a run stopped with an error (%d, %d)\n", err[0], err[1]);
        return 1;
    }

    printf("OR1 setting (expected)        0:30        1:07        2:00   minutes at 0.2 (expected 15)\n");
    for (d = 0; d < 2; d++)
    {
        printf("  %-22s", name[d]);
        for (i = 0; i < 3; i++)
        {
            printf("  %4.2f (%3.1f)", set[d][i], expSet[i]);
            if (fabs(set[d][i] - expSet[i]) > 1e-6) ok = 0;
        }
        printf("   %5.1f\n", minutes[d]);
        if (fabs(minutes[d] - 15.0) > 0.5 + 1e-6) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: rule 'IF GAGE G2 INTENSITY > 0.1' did not act while G2 recorded 0.5 in/hr"
               " (OR1 at 0.2 for %.1f and %.1f min instead of 15)\n", minutes[0], minutes[1]);
        return 1;
    }
    printf("PASS: the rule reads G2's rainfall and throttles OR1 from 01:00 to 01:15 in both decks\n");
    return 0;
}
