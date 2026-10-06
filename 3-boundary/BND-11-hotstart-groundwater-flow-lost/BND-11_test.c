/*
 * BND-11: after a hot start the groundwater inflow to the drainage system
 * restarts from 0 and ramps up over the first runoff step.
 *
 * A 100-acre aquifer drains steadily to junction J1 (no rain, so J1's lateral
 * inflow is the groundwater flow only). BND-11_full.inp runs 0:00-12:00 in
 * one go. BND-11_save.inp runs 0:00-6:00 and saves a hot start file;
 * BND-11_use.inp continues 6:00-12:00 from it. A hot start exists to make
 * the continuation reproduce the uninterrupted run, so J1's lateral inflow
 * after 6:00 must be the same in both.
 *
 * The test records J1's lateral inflow (swmm_getValue) at every routing step,
 * prints it at 6:15-7:00 and integrates it over 6:00-7:00 (the first 1-hour
 * runoff step) and 6:00-12:00. Tolerance 1 % of the 6:00-7:00 volume: with
 * the bug about half of that hour's groundwater volume is missing.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#define TOL_PCT 1.0

/* clock hours of the 4 printed times */
static const double printAt[4] = {6.25, 6.5, 6.75, 7.0};

/* runs a deck; startHour = clock time of the deck's start.
   Returns the error code; fills q[] at printAt[] (cfs) and the lateral
   inflow volumes (ft3) for 6:00-7:00 and 6:00-12:00. */
static int runDeck(const char* inp, const char* rpt, const char* out,
                   double startHour, double q[4], double* v1, double* v6)
{
    double elapsed = 0.0, tPrev = startHour, t, lat;
    int err, j1, k = 0;

    *v1 = *v6 = 0.0;
    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    if (err) { swmm_close(); return err; }
    j1 = swmm_getIndex(swmm_NODE, "J1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        t = startHour + elapsed * 24.0;
        lat = swmm_getValue(swmm_NODE_LATFLOW, j1);
        if (t > 6.0 + 1.0e-6)
        {
            double dt = (t - (tPrev > 6.0 ? tPrev : 6.0)) * 3600.0;
            *v6 += lat * dt;
            if (t <= 7.0 + 1.0e-6) *v1 += lat * dt;
        }
        while (k < 4 && t >= printAt[k] - 1.0e-6) q[k++] = lat;
        tPrev = t;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    return err;
}

int main(void)
{
    double qf[4] = {0}, qh[4] = {0}, qa[4] = {0};
    double vf1, vf6, vh1, vh6, va1, va6, d1;
    int err, k;

    err = runDeck("BND-11_full.inp", "BND-11_full.rpt", "BND-11_full.out",
                  0.0, qf, &vf1, &vf6);
    if (!err) err = runDeck("BND-11_save.inp", "BND-11_save.rpt",
                            "BND-11_save.out", 0.0, qa, &va1, &va6);
    if (!err) err = runDeck("BND-11_use.inp", "BND-11_use.rpt",
                            "BND-11_use.out", 6.0, qh, &vh1, &vh6);
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
