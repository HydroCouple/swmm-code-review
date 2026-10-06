/*
 * BND-11 for 6.0.0: after a hot start the groundwater inflow to the
 * drainage system restarts from 0 and ramps up over the first runoff step.
 *
 * A 100-acre aquifer drains steadily to junction J1 (no rain, so J1's lateral
 * inflow is the groundwater flow only). BND-11_full.inp runs 0:00-12:00 in
 * one go. BND-11_save.inp runs 0:00-6:00 and saves a hot start file;
 * BND-11_use.inp continues 6:00-12:00 from it. A hot start exists to make
 * the continuation reproduce the uninterrupted run, so J1's lateral inflow
 * after 6:00 must be the same in both.
 *
 * 6.0.0 cannot continue this model through [FILES] SAVE/USE HOTSTART: it
 * writes a routing-only legacy file and then fails to read it back (see the
 * README). The test therefore uses 6.0.0's native hot start API, which
 * carries the aquifer state: BND-11_save.inp is run and its state saved with
 * swmm_hotstart_save(); the continuation is BND-11_use.inp without its
 * [FILES] section (written as BND-11_cont6.inp), with swmm_hotstart_apply().
 * (The outfall is named OUT01 so that the native file is 260 bytes:
 * swmm_hotstart_open() reads the trailing CRC with a misaligned uint32 load,
 * which UBSan reports whenever the file size is not a multiple of 4.)
 *
 * The test records J1's lateral inflow at every routing step,
 * prints it at 6:15-7:00 and integrates it over 6:00-7:00 (the first 1-hour
 * runoff step) and 6:00-12:00. Tolerance 1 % of the 6:00-7:00 volume: with
 * the bug about half of that hour's groundwater volume is missing.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_hotstart.h"

#define TOL_PCT 1.0

/* clock hours of the 4 printed times */
static const double printAt[4] = {6.25, 6.5, 6.75, 7.0};

/* writes a copy of the deck without its [FILES] section */
static int dropFiles(const char* base, const char* inp)
{
    char line[512];
    int skip = 0;
    FILE* in = fopen(base, "r");
    FILE* out = fopen(inp, "w");
    if (!in || !out)
    {
        if (in) fclose(in);
        if (out) fclose(out);
        return 0;
    }
    while (fgets(line, sizeof(line), in))
    {
        if (line[0] == '[') skip = (strncmp(line, "[FILES]", 7) == 0);
        if (!skip) fputs(line, out);
    }
    fclose(in);
    fclose(out);
    return 1;
}

/* runs a deck; startHour = clock time of the deck's start; hsUse = native
   hot start file applied before the start, hsSave = file the final state is
   saved to (either may be NULL). Returns the error code; fills q[] at
   printAt[] (cfs) and the lateral inflow volumes (ft3) for 6:00-7:00 and
   6:00-12:00. */
static int runDeck(const char* inp, const char* rpt, const char* out,
                   double startHour, const char* hsUse, const char* hsSave,
                   double q[4], double* v1, double* v6)
{
    double elapsed = 0.0, tPrev = startHour, t, lat = 0.0;
    int err, j1, k = 0;
    SWMM_Engine e = swmm_engine_create();

    *v1 = *v6 = 0.0;
    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err && hsUse)
    {
        SWMM_HotStart hs = NULL;
        err = swmm_hotstart_open(hsUse, &hs);
        if (!err) err = swmm_hotstart_apply(e, hs);
        if (hs) swmm_hotstart_close(hs);
    }
    if (!err) err = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    while (!err)
    {
        err = swmm_engine_step(e, &elapsed);
        if (err || elapsed <= 0.0) break;
        t = startHour + elapsed * 24.0;
        swmm_node_get_lateral_inflow(e, j1, &lat);
        if (t > 6.0 + 1.0e-6)
        {
            double dt = (t - (tPrev > 6.0 ? tPrev : 6.0)) * 3600.0;
            *v6 += lat * dt;
            if (t <= 7.0 + 1.0e-6) *v1 += lat * dt;
        }
        while (k < 4 && t >= printAt[k] - 1.0e-6) q[k++] = lat;
        tPrev = t;
    }
    if (!err && hsSave) err = swmm_hotstart_save(e, hsSave);
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    double qf[4] = {0}, qh[4] = {0}, qa[4] = {0};
    double vf1, vf6, vh1, vh6, va1, va6, d1;
    int err, k;

    if (!dropFiles("BND-11_use.inp", "BND-11_cont6.inp"))
    {
        printf("FAIL: could not write BND-11_cont6.inp\n");
        return 1;
    }
    err = runDeck("BND-11_full.inp", "BND-11_full6.rpt", "BND-11_full6.out",
                  0.0, NULL, NULL, qf, &vf1, &vf6);
    if (!err) err = runDeck("BND-11_save.inp", "BND-11_save6.rpt",
                            "BND-11_save6.out", 0.0, NULL, "BND-11_6.hs",
                            qa, &va1, &va6);
    if (!err) err = runDeck("BND-11_cont6.inp", "BND-11_cont6.rpt",
                            "BND-11_cont6.out", 6.0, "BND-11_6.hs", NULL,
                            qh, &vh1, &vh6);
    if (err)
    {
        printf("FAIL: a run stopped with error %d\n", err);
        return 1;
    }

    printf("J1 lateral (groundwater) inflow, cfs\n");
    printf("Time    Uninterrupted   Hot-started\n");
    for (k = 0; k < 4; k++)
        printf("%2d:%02d   %13.3f   %11.3f\n", (int)printAt[k],
               (int)((printAt[k] - (int)printAt[k]) * 60.0 + 0.5), qf[k], qh[k]);
    printf("GW volume to J1, ac-ft\n");
    printf("6:00-7:00    %10.4f   %11.4f\n", vf1 / 43560.0, vh1 / 43560.0);
    printf("6:00-12:00   %10.4f   %11.4f\n", vf6 / 43560.0, vh6 / 43560.0);

    d1 = 100.0 * (vh1 - vf1) / vf1;
    if (fabs(d1) > TOL_PCT)
    {
        printf("FAIL: after the hot start J1 receives %.4f ac-ft of groundwater "
               "in the first hour instead of %.4f ac-ft (%+.1f %%)\n",
               vh1 / 43560.0, vf1 / 43560.0, d1);
        return 1;
    }
    printf("PASS: the hot-started run delivers the same groundwater inflow as "
           "the uninterrupted run (6:00-7:00 %+.2f %%)\n", d1);
    return 0;
}
