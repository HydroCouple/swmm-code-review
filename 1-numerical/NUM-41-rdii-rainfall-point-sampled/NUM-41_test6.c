/*
 * NUM-41 for 6.0.0: RDII takes one point sample of the gage's rain intensity
 * per rain interval instead of the rain that fell in it. 6.0.0 computes RDII
 * during the run but reproduces the legacy sampling; this is the same check
 * as NUM-41_test.c through the 6.0.0 C API.
 *
 * NUM-41_burst.inp puts 1.0 in of rain in one 15-minute gage record (0:15 to
 * 0:30) on an RDII unit hydrograph with R = 0.1 over a 100 ac sewershed.
 * Whatever the time steps, the RDII volume entering node J1 must be
 *     R x P x A = 0.1 x 1 in x 100 ac = 10 ac-in = 36,300 ft3.
 * The test runs the deck with WET_STEP = 1, 2, ..., 15 min (it rewrites the
 * WET_STEP line into NUM-41_ws.inp) and integrates J1's lateral inflow, which
 * is RDII only. It also runs NUM-41_short-limb.inp, whose UH falling limb
 * (6 min) is shorter than WET_STEP (10 min), so RDII splits each 10-min step
 * into 6-min rain intervals.
 *
 * Tolerances: a WET_STEP that divides 15 min gives the exact volume; the
 * others give 0.67 to 1.20 times it unpatched (the smallest error is 6.7%),
 * and within 0.4% with the fix (the UH convolution's own discretisation).
 * So the burst runs must be within 2%. In the short-limb run the RDII flow is
 * held for 10 min, longer than the 6-min falling limb, which adds about 3% on
 * its own; that run is checked to 5% (unpatched it is 58% low).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

#define EXPECTED_FT3 (0.1 * (1.0 / 12.0) * 100.0 * 43560.0)

/* copy the template deck, replacing its WET_STEP line */
static int makeDeck(const char* tmpl, const char* out, int wetMin)
{
    char line[512];
    FILE* fi = fopen(tmpl, "r");
    FILE* fo = fopen(out, "w");
    if (!fi || !fo) { if (fi) fclose(fi); if (fo) fclose(fo); return 0; }
    while (fgets(line, sizeof(line), fi))
    {
        if (strncmp(line, "WET_STEP", 8) == 0)
            fprintf(fo, "WET_STEP         00:%02d:00\n", wetMin);
        else fputs(line, fo);
    }
    fclose(fi);
    fclose(fo);
    return 1;
}

/* run a deck and return the volume (ft3) of lateral inflow into J1 */
static double rdiiVolume(const char* inp, int* err)
{
    double t = 0.0, tOld = 0.0, vol = 0.0, q = 0.0;
    int j = -1;
    SWMM_Engine e = swmm_engine_create();

    *err = swmm_engine_open(e, inp, "NUM-41_6.rpt", "NUM-41_6.out", NULL);
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
    int wet, err, nBad = 0;
    double v, ratio, worst = 1.0;

    printf("Expected RDII volume R x P x A = 0.1 x 1 in x 100 ac = %.0f ft3\n\n",
           EXPECTED_FT3);
    printf("deck        WET_STEP  RDII volume (ft3)  ratio\n");
    for (wet = 1; wet <= 15; wet++)
    {
        if (!makeDeck("NUM-41_burst.inp", "NUM-41_ws.inp", wet))
        {
            printf("FAIL: cannot write NUM-41_ws.inp\n");
            return 1;
        }
        v = rdiiVolume("NUM-41_ws.inp", &err);
        if (err) { printf("FAIL: run with WET_STEP %d min stopped with error %d\n", wet, err); return 1; }
        ratio = v / EXPECTED_FT3;
        printf("burst       %2d min    %10.1f         %.3f%s\n", wet, v, ratio,
               fabs(ratio - 1.0) > 0.02 ? "  <-- wrong" : "");
        if (fabs(ratio - 1.0) > 0.02) nBad++;
        if (fabs(ratio - 1.0) > fabs(worst - 1.0)) worst = ratio;
    }
    v = rdiiVolume("NUM-41_short-limb.inp", &err);
    if (err) { printf("FAIL: short-limb run stopped with error %d\n", err); return 1; }
    ratio = v / EXPECTED_FT3;
    printf("short-limb  10 min    %10.1f         %.3f%s\n\n", v, ratio,
           fabs(ratio - 1.0) > 0.05 ? "  <-- wrong" : "");
    if (fabs(ratio - 1.0) > 0.05) nBad++;
    if (fabs(ratio - 1.0) > fabs(worst - 1.0)) worst = ratio;

    if (nBad)
    {
        printf("FAIL: %d of 16 runs put the wrong rain volume into RDII "
               "(worst: %.3f x R*P*A)\n", nBad, worst);
        return 1;
    }
    printf("PASS: RDII volume is R x P x A for every WET_STEP "
           "(worst: %.3f x R*P*A)\n", worst);
    return 0;
}
