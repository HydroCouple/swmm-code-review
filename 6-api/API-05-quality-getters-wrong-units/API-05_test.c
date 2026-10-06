/*
 * API-05: units of the 5.3.0 quality getters/setters
 *   swmm_SUBCATCH_POLLUTANT_PONDED_CONCENTRATION (get and set) and
 *   swmm_LINK_POLLUTANT_LOAD (get).
 *
 * Deck: 10 ac fully impervious S1, 1.00 in of rain (1 in/hr, 0:15-1:15)
 * carrying P1 at 10 mg/L, depression storage 0.10 in, no evaporation, no
 * buildup or washoff. P2 has no source.
 *
 * Expected from first principles:
 *  1. Before the rain the surface is dry: ponded concentration 0.
 *  2. The ponded water is rain only, so its P1 concentration is the rain
 *     concentration, 10 mg/L (checked within 5%).
 *  3. Setting P2's ponded concentration to 100 mg/L at 0:45 must put that
 *     mass in the ponded water, so S1's P2 runoff concentration over the next
 *     steps is close to 100 mg/L. The rain that falls in one 1-minute step
 *     (0.017 in) dilutes the ~0.23 in of ponded water by about 7%; the check
 *     asks for more than 50 mg/L.
 *  4. The P1 load carried by C1 over the run is the P1 in the rain that runs
 *     off: 10 mg/L x 28.3168 L/ft3 x (0.90 in / 12) x 435,600 ft2
 *     = 9.2511e6 mg = 20.395 lb, which is also what the Link Pollutant Load
 *     Summary reports. Checked within 3% (0.2% of the rain is still ponded
 *     above the depression storage after 4 h).
 * The defects are off by factors of ~1e6 (ponded concentration) and ~1.6e4
 * (link load), so the tolerances cannot be confused with them.
 *
 * 5.2.4 has no expanded getters/setters and is not affected.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    const double LperFT3 = 28.316846592, LBperMG = 2.204622621848776e-6;
    const double loadExpected = 10.0 * LperFT3 * (0.90 / 12.0) * 10.0 * 43560.0 * LBperMG;
    double elapsed = 0.0, tMin;
    double dryPonded = -1.0, ponded1 = 0.0, runoff1 = 0.0, back2 = 0.0, runoff2 = 0.0;
    double linkLoad = 0.0, subLoad = 0.0, c;
    int err, s1, c1, step = 0, setStep = -1, rcSet = 0, bad = 0;

    err = swmm_open("API-05_rain-quality.inp", "API-05.rpt", "API-05.out");
    if (!err) err = swmm_start(1);
    s1 = swmm_getIndex(swmm_SUBCATCH, "S1");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        step++;
        tMin = elapsed * 1440.0;
        if (step == 1)
            dryPonded = swmm_getValueExpanded(swmm_SUBCATCH,
                swmm_SUBCATCH_POLLUTANT_PONDED_CONCENTRATION, s1, 0, 0);
        if (setStep < 0 && tMin >= 45.0 - 1e-6)
        {
            ponded1 = swmm_getValueExpanded(swmm_SUBCATCH,
                swmm_SUBCATCH_POLLUTANT_PONDED_CONCENTRATION, s1, 0, 0);
            runoff1 = swmm_getValueExpanded(swmm_SUBCATCH,
                swmm_SUBCATCH_POLLUTANT_RUNOFF_CONCENTRATION, s1, 0, 0);
            rcSet = swmm_setValueExpanded(swmm_SUBCATCH,
                swmm_SUBCATCH_POLLUTANT_PONDED_CONCENTRATION, s1, 0, 1, 100.0);
            back2 = swmm_getValueExpanded(swmm_SUBCATCH,
                swmm_SUBCATCH_POLLUTANT_PONDED_CONCENTRATION, s1, 0, 1);
            setStep = step;
        }
        else if (setStep > 0 && step <= setStep + 5)
        {
            c = swmm_getValueExpanded(swmm_SUBCATCH,
                swmm_SUBCATCH_POLLUTANT_RUNOFF_CONCENTRATION, s1, 0, 1);
            if (c > runoff2) runoff2 = c;
        }
        linkLoad = swmm_getValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_LOAD, c1, 0, 0);
        subLoad = swmm_getValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_POLLUTANT_TOTAL_LOAD,
                                        s1, 0, 0);
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err) { printf("FAIL: run error %d\n", err); return 1; }

    printf("%-58s %14s %14s\n", "quantity", "API", "expected");
    printf("%-58s %14.6g %14s\n", "S1 P1 ponded conc. at 0:01, dry surface (mg/L)",
           dryPonded, "0");
    printf("%-58s %14.6g %14.4f\n", "S1 P1 ponded conc. at 0:45, rain-only water (mg/L)",
           ponded1, 10.0);
    printf("%-58s %14.6g %14.4f\n", "  (S1 P1 runoff conc. at 0:45, mg/L)", runoff1, 10.0);
    printf("%-58s %14.6g %14s\n", "S1 P2 ponded conc. set to 100 at 0:45, read back",
           back2, "100");
    printf("%-58s %14.6g %14s\n", "S1 P2 max runoff conc. in the next 5 steps (mg/L)",
           runoff2, "> 50");
    printf("%-58s %14.6g %14.4f\n", "C1 P1 load (LINK_POLLUTANT_LOAD, lb)", linkLoad,
           loadExpected);
    printf("%-58s %14.6g %14.4f\n", "  (S1 P1 load, SUBCATCH_POLLUTANT_TOTAL_LOAD, lb)",
           subLoad, loadExpected);
    printf("set returned %d\n", rcSet);

    if (!(fabs(dryPonded) < 1e-9))
    {
        printf("-> the ponded concentration of a dry surface is not 0\n"); bad++;
    }
    if (!(fabs(ponded1 - 10.0) < 0.5))
    {
        printf("-> the ponded concentration getter is not in mg/L (%.6g vs 10)\n", ponded1);
        bad++;
    }
    if (rcSet != 0 || !(runoff2 > 50.0 && runoff2 <= 100.0))
    {
        printf("-> the ponded concentration setter does not put 100 mg/L in the ponded "
               "water (runoff %.6g mg/L)\n", runoff2);
        bad++;
    }
    if (!(fabs(linkLoad - loadExpected) < 0.03 * loadExpected))
    {
        printf("-> the link load getter is not in lb (%.6g vs %.4f)\n", linkLoad, loadExpected);
        bad++;
    }
    if (bad)
    {
        printf("FAIL: %d of 4 quality API checks wrong: ponded concentration %.6g mg/L "
               "(expected 10), runoff after setting 100 mg/L %.6g mg/L, C1 load %.6g "
               "(expected %.3f lb)\n", bad, ponded1, runoff2, linkLoad, loadExpected);
        return 1;
    }
    printf("PASS: ponded concentration get/set are in mg/L (0 when dry) and the link load "
           "is in lb\n");
    return 0;
#else
    printf("5.2.4 has no swmm_getValueExpanded/swmm_setValueExpanded\n");
    printf("PASS: not affected (the API does not exist in 5.2.4)\n");
    return 0;
#endif
}
