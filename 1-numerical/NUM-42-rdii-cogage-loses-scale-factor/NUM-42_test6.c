/*
 * NUM-42 for 6.0.0: an RDII unit hydrograph whose gage shares its rain series
 * with an earlier gage loses the gage's rain scale factor in 5.3.0. 6.0.0
 * resolves each UH group's own gage, so this test is expected to pass.
 *
 * In both decks the UH (R = 0.1) is on gage G2, which reads series TS1 (1.0 in
 * in the first hour) with the 5.3.0 rain scale factor 2.0. Its 100 ac must
 * produce R x P x SF x A = 0.1 x 1 in x 2.0 x 100 ac = 20 ac-in = 72,600 ft3
 * of RDII at node J1 (J1 receives nothing else).
 *   NUM-42_own-series.inp     G2 is the only gage on TS1 (control)
 *   NUM-42_shared-series.inp  G1 (scale 1.0, declared first, used by a
 *                             subcatchment that drains elsewhere) also
 *                             reads TS1, so G2 becomes G1's co-gage
 * The test integrates J1's lateral inflow over the run.
 *
 * Tolerance 1%: the bug halves
 * the volume, and the UH convolution of a 1-hour record on a 15-min step is
 * exact to well within that.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

/* run a deck and return the volume (ft3) of lateral inflow into J1 */
static double rdiiVolume(const char* inp, int* err)
{
    double t = 0.0, tOld = 0.0, vol = 0.0, q = 0.0;
    int j = -1;
    SWMM_Engine e = swmm_engine_create();

    *err = swmm_engine_open(e, inp, "NUM-42_6.rpt", "NUM-42_6.out", NULL);
    if (!*err) *err = swmm_engine_initialize(e);
    if (!*err) *err = swmm_engine_start(e, 1);
    if (!*err) j = swmm_node_index(e, "J1");
    while (!*err)
    {
        *err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_node_get_lateral_inflow(e, j, &q);     /* project units: cfs */
        vol += q * (t - tOld) * 86400.0;
        tOld = t;
    }
    if (!*err) *err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return vol;
}

int main(void)
{
    const char* decks[2] = {"NUM-42_own-series.inp", "NUM-42_shared-series.inp"};
    double sf = 2.0, expected, v[2];
    int i, err, ok = 1;

    expected = 0.1 * (1.0 / 12.0) * sf * 100.0 * 43560.0;
    printf("Expected RDII volume R x P x SF x A = 0.1 x 1 in x %.1f x 100 ac = %.0f ft3\n\n",
           sf, expected);
    printf("deck                       RDII volume (ft3)  ratio\n");
    for (i = 0; i < 2; i++)
    {
        v[i] = rdiiVolume(decks[i], &err);
        if (err) { printf("FAIL: %s stopped with error %d\n", decks[i], err); return 1; }
        printf("%-26s %10.1f         %.3f\n", decks[i], v[i], v[i] / expected);
        if (fabs(v[i] / expected - 1.0) > 0.01) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: RDII on gage G2 is %.0f ft3 when G2 shares its series with G1 "
               "and %.0f ft3 when it does not (expected %.0f)\n", v[1], v[0], expected);
        return 1;
    }
    printf("PASS: RDII on G2 is R x P x SF x A whether or not G2 shares its series\n");
    return 0;
}
