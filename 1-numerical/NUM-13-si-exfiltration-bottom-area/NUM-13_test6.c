/*
 * NUM-13 for 6.0.0: the seepage bottom area of FUNCTIONAL, CYLINDRICAL, CONICAL and
 * PYRAMIDAL storage units is left in user units (m2 used as ft2 under SI),
 * Exfiltration.cpp, as legacy exfil.c.
 *
 * Same decks and check as NUM-13_test.c: the same 100 m2 basin over the same
 * soil must lose the same volume (within 1%) whether it is described as
 * FUNCTIONAL, TABULAR or CYLINDRICAL, in CMS or in CFS.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

#define M3_PER_FT3 0.0283168

static int run(const char *inp, const char *rpt, const char *out,
               double m3per, double loss[3])
{
    const char *ids[3] = {"SU1", "SU2", "SU3"};
    double t = 0.0, v0[3], v[3];
    int i, k[3], err;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    for (i = 0; i < 3 && !err; i++)
    {
        k[i] = swmm_node_index(e, ids[i]);
        swmm_node_get_volume(e, k[i], &v0[i]);
        v[i] = v0[i];
    }
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (err) break;
        for (i = 0; i < 3; i++) swmm_node_get_volume(e, k[i], &v[i]);
        if (t <= 0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    for (i = 0; i < 3; i++) loss[i] = (v0[i] - v[i]) * m3per;
    return err;
}

int main(void)
{
    const char *shape[3] = {"FUNCTIONAL", "TABULAR", "CYLINDRICAL"};
    double si[3] = {0}, us[3] = {0}, lo = 1e30, hi = 0.0;
    int i, err;

    err = run("NUM-13_si.inp", "NUM-13_si6.rpt", "NUM-13_si6.out", 1.0, si);
    if (!err) err = run("NUM-13_us.inp", "NUM-13_us6.rpt", "NUM-13_us6.out",
                        M3_PER_FT3, us);
    if (err)
    {
        printf("FAIL: a run stopped with error %d\n", err);
        return 1;
    }

    printf("Seepage loss over 6 h (m3)\n");
    printf("Shape          SI (CMS)   US (CFS)\n");
    for (i = 0; i < 3; i++)
    {
        printf("%-12s  %9.3f  %9.3f\n", shape[i], si[i], us[i]);
        if (si[i] < lo) lo = si[i];
        if (si[i] > hi) hi = si[i];
        if (us[i] < lo) lo = us[i];
        if (us[i] > hi) hi = us[i];
    }
    if (hi > 1.01 * lo)
    {
        printf("FAIL: the same basin loses between %.3f and %.3f m3 "
               "(%.1f%% apart) depending on shape and units\n",
               lo, hi, 100.0 * (hi - lo) / hi);
        return 1;
    }
    printf("PASS: the same basin loses the same volume (%.3f m3) for every "
           "shape and unit system\n", lo);
    return 0;
}
