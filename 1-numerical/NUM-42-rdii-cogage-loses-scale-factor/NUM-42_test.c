/*
 * NUM-42: an RDII unit hydrograph whose gage shares its rain series with an
 * earlier gage loses the gage's rain scale factor (5.3.0 regression).
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
 * 5.2.4 has no rain scale factor (it ignores the extra token), so there the
 * expected volume is the unscaled 36,300 ft3. Tolerance 1%: the bug halves
 * the volume, and the UH convolution of a 1-hour record on a 15-min step is
 * exact to well within that.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

/* run a deck and return the volume (ft3) of lateral inflow into J1 */
static double rdiiVolume(const char* inp, int* err)
{
    double t = 0.0, tOld = 0.0, vol = 0.0;
    int j;

    *err = swmm_open(inp, "NUM-42.rpt", "NUM-42.out");
    if (!*err) *err = swmm_start(1);
    j = swmm_getIndex(swmm_NODE, "J1");
    while (!*err)
    {
        *err = swmm_step(&t);
        if (t <= 0.0) break;
        vol += swmm_getValue(swmm_NODE_LATFLOW, j) * (t - tOld) * 86400.0;
        tOld = t;
    }
    swmm_end();
    swmm_close();
    return vol;
}

int main(void)
{
    const char* decks[2] = {"NUM-42_own-series.inp", "NUM-42_shared-series.inp"};
    double sf = 2.0, expected, v[2];
    int i, err, ok = 1;

    if (swmm_getVersion() < 53000)
    {
        sf = 1.0;
        printf("SWMM %d has no rain scale factor: expecting the unscaled volume\n",
               swmm_getVersion());
    }
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
