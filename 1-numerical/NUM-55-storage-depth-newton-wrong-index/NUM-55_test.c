/*
 * NUM-55: 5.2.4's storage_getDepth() solves for depth with another node's
 * geometry, and mixes user and internal units, for FUNCTIONAL, CONICAL and
 * PYRAMIDAL storage units.
 *
 * Kinematic wave routing gets a storage unit's depth from its volume
 * (storage_getDepth()). Each deck fills two storage units with a constant
 * inflow for one hour, with their outlets above the water:
 *   SF  FUNCTIONAL  Area = 50 + 10 d^2       V(d) = 50 d + (10/3) d^3
 *   SP  PYRAMIDAL   20 x 10 base, slope 2     V(d) = 200 d + 60 d^2 + (16/3) d^3
 * NUM-55_cms.inp is in metres (storage units are the first nodes);
 * NUM-55_cfs-junction-first.inp is in feet with a junction listed first, so
 * a storage unit's node index and storage index differ.
 *
 * Correct behaviour: the depth reported for a unit is the one at which its
 * own shape holds the volume reported for it, V(depth) = volume.
 *
 * Tolerance: 1% of the volume. The Newton solve stops within 0.001 of the
 * depth, about 0.15% of the volume here; the defect gives depths 10 times
 * too small (metres) or another unit's depth (feet).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static double volSF(double d) { return 50.0 * d + 10.0 / 3.0 * d * d * d; }
static double volSP(double d) { return 200.0 * d + 60.0 * d * d + 16.0 / 3.0 * d * d * d; }

static int check(const char *inp, const char *rpt, const char *out,
                 const char *units)
{
    const char *ids[2] = {"SF", "SP"};
    double t = 0.0, v[2] = {0, 0}, d[2] = {0, 0}, vd;
    int i, k[2], err, bad = 0;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    for (i = 0; i < 2 && !err; i++) k[i] = swmm_getIndex(swmm_NODE, ids[i]);
    while (!err)
    {
        err = swmm_step(&t);
        if (err) break;
        for (i = 0; i < 2; i++)
        {
            v[i] = swmm_getValue(swmm_NODE_VOLUME, k[i]);
            d[i] = swmm_getValue(swmm_NODE_DEPTH, k[i]);
        }
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_report();
    swmm_close();
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
    int bad = check("NUM-55_cms.inp", "NUM-55_cms.rpt", "NUM-55_cms.out", "m")
            + check("NUM-55_cfs-junction-first.inp", "NUM-55_cfs.rpt",
                    "NUM-55_cfs.out", "ft");
    if (bad)
    {
        printf("FAIL: %d of 4 storage units report a depth at which their "
               "shape does not hold their volume\n", bad);
        return 1;
    }
    printf("PASS: every storage unit's depth matches its volume\n");
    return 0;
}
