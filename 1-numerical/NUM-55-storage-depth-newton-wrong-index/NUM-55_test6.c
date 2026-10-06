/*
 * NUM-55 for 6.0.0: 5.2.4's storage_getDepth() solves for depth with another node's
 * geometry, and mixes user and internal units, for FUNCTIONAL, CONICAL and
 * PYRAMIDAL storage units.
 *
 * 6.0.0 has its own storage_getDepth (Node.cpp) with the corrected unit
 * handling and node index, so this test passes unpatched. Same decks and check
 * as NUM-55_test.c: each unit's reported depth d must satisfy
 * V(d) = reported volume within 1%, for
 *   SF  FUNCTIONAL  V(d) = 50 d + (10/3) d^3
 *   SP  PYRAMIDAL   V(d) = 200 d + 60 d^2 + (16/3) d^3
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

static double volSF(double d) { return 50.0 * d + 10.0 / 3.0 * d * d * d; }
static double volSP(double d) { return 200.0 * d + 60.0 * d * d + 16.0 / 3.0 * d * d * d; }

static int check(const char *inp, const char *rpt, const char *out,
                 const char *units)
{
    const char *ids[2] = {"SF", "SP"};
    double t = 0.0, v[2] = {0, 0}, d[2] = {0, 0}, vd;
    int i, k[2], err, bad = 0;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    for (i = 0; i < 2 && !err; i++) k[i] = swmm_node_index(e, ids[i]);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (err) break;
        for (i = 0; i < 2; i++)
        {
            swmm_node_get_volume(e, k[i], &v[i]);
            swmm_node_get_depth(e, k[i], &d[i]);
        }
        if (t <= 0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("%s: run stopped with error %d\n", inp, err);
        return 2;
    }
    printf("%s\n", inp);
    for (i = 0; i < 2; i++)
    {
        vd = (i == 0) ? volSF(d[i]) : volSP(d[i]);
        printf("  %s  node %d  volume %9.3f %s3  depth %7.4f %s  "
               "V(depth) %9.3f%s\n", ids[i], k[i], v[i], units, d[i], units,
               vd, fabs(vd - v[i]) > 0.01 * v[i] ? "   <-- wrong depth" : "");
        if (fabs(vd - v[i]) > 0.01 * v[i]) bad++;
    }
    return bad;
}

int main(void)
{
    int bad = check("NUM-55_cms.inp", "NUM-55_cms6.rpt", "NUM-55_cms6.out", "m")
            + check("NUM-55_cfs-junction-first.inp", "NUM-55_cfs6.rpt",
                    "NUM-55_cfs6.out", "ft");
    if (bad)
    {
        printf("FAIL: %d of 4 storage units report a depth at which their "
               "shape does not hold their volume\n", bad);
        return 1;
    }
    printf("PASS: every storage unit's depth matches its volume\n");
    return 0;
}
