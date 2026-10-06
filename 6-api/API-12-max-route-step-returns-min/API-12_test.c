/*
 * API-12: swmm_getValue(swmm_MAXROUTESTEP) always returns MinRouteStep.
 *
 * getMaxRouteStep() asks the dynamic wave solver for its Courant-limited step
 * with a Courant factor of 1, but passes MinRouteStep (0.5 s by default) as
 * the upper limit of the search. dynwave_getRoutingStep() can only lower that
 * limit and then clamps the result to MinRouteStep, so the answer is always
 * MinRouteStep, whatever the flow.
 *
 * Correct behaviour, from the solver's own stability rule: the step the
 * engine takes is the Courant step with VARIABLE_STEP = 0.75, capped at
 * ROUTING_STEP; the maximum stable step uses a Courant factor of 1 and the
 * same cap, so it can never be shorter than the next step the engine takes
 * from the same state. The test reads MAXROUTESTEP after swmm_start and after
 * every step, and compares it with the length of the step that follows
 * (difference of the elapsed times, 1 ms tolerance for the solver's
 * millisecond rounding). With the bug the value is 0.5 s while the engine
 * takes steps of up to 30 s.
 */
#include <stdio.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, tPrev = 0.0, maxStep, dt, routeStep;
    double dtMin = 1e30, dtMax = 0.0, msMin = 1e30, msMax = 0.0;
    double firstT = 0.0, firstDt = 0.0, firstMs = 0.0;
    double dtPeak = 1e30, msPeak = 0.0;     /* shortest step after the first */
    int err, steps = 0, longer = 0;

    err = swmm_open("API-12_variable-step.inp", "API-12.rpt", "API-12.out");
    if (!err) err = swmm_start(0);
    if (err)
    {
        printf("FAIL: could not start the run (error %d)\n", err);
        return 1;
    }
    routeStep = swmm_getValue(swmm_ROUTESTEP, 0);
    maxStep = swmm_getValue(swmm_MAXROUTESTEP, 0);   /* before the first step */
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        dt = (t - tPrev) * 86400.0;
        steps++;
        if (dt < dtMin) dtMin = dt;
        if (dt > dtMax) dtMax = dt;
        if (maxStep < msMin) msMin = maxStep;
        if (maxStep > msMax) msMax = maxStep;
        if (steps > 1 && dt < dtPeak) { dtPeak = dt; msPeak = maxStep; }
        if (dt > maxStep + 1.0e-3)
        {
            if (longer == 0) { firstT = tPrev; firstDt = dt; firstMs = maxStep; }
            longer++;
        }
        tPrev = t;
        maxStep = swmm_getValue(swmm_MAXROUTESTEP, 0);
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("ROUTING_STEP                         %8.3f s\n", routeStep);
    printf("steps taken                          %8d\n", steps);
    printf("step lengths taken                   %8.3f to %.3f s\n", dtMin, dtMax);
    printf("MAXROUTESTEP reported before a step  %8.3f to %.3f s\n", msMin, msMax);
    printf("shortest step after the first        %8.3f s; MAXROUTESTEP before it %.3f s\n",
           dtPeak, msPeak);
    printf("steps longer than the MAXROUTESTEP\n"
           "  reported just before them          %8d of %d\n", longer, steps);
    if (longer)
    {
        printf("  first: at %.4f h the engine took %.3f s; MAXROUTESTEP was %.3f s\n",
               firstT * 24.0, firstDt, firstMs);
        printf("FAIL: MAXROUTESTEP (%.3f to %.3f s) is shorter than the steps the "
               "engine takes (up to %.3f s)\n", msMin, msMax, dtMax);
        return 1;
    }
    printf("PASS: MAXROUTESTEP is never shorter than the next step the engine takes\n");
    return 0;
}
