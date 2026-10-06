/*
 * API-12 for 6.0.0: the routing step the API reports is an upper bound on the
 * steps the engine takes.
 *
 * 6.0.0 has no counterpart of the legacy swmm_MAXROUTESTEP (a Courant-limited
 * maximum step); its only step getter is swmm_get_routing_step(), which
 * returns the ROUTING_STEP option. The test checks the same property as the
 * legacy test for that getter: read it after swmm_engine_start and after every
 * step, and verify that the step that follows (difference of the elapsed
 * times) is never longer than the value read, with 1 ms tolerance.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"

int main(void)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, tPrev = 0.0, maxStep = 0.0, dt;
    double dtMin = 1e30, dtMax = 0.0, msMin = 1e30, msMax = 0.0;
    int err, steps = 0, longer = 0;

    err = swmm_engine_open(e, "API-12_variable-step.inp", "API-12_6.rpt", "API-12_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 0);
    if (err)
    {
        printf("FAIL: could not start the run (error %d)\n", err);
        return 1;
    }
    swmm_get_routing_step(e, &maxStep);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        dt = (t - tPrev) * 86400.0;
        steps++;
        if (dt < dtMin) dtMin = dt;
        if (dt > dtMax) dtMax = dt;
        if (maxStep < msMin) msMin = maxStep;
        if (maxStep > msMax) msMax = maxStep;
        if (dt > maxStep + 1.0e-3) longer++;
        tPrev = t;
        swmm_get_routing_step(e, &maxStep);
    }
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("steps taken                               %8d\n", steps);
    printf("step lengths taken                        %8.3f to %.3f s\n", dtMin, dtMax);
    printf("swmm_get_routing_step before a step       %8.3f to %.3f s\n", msMin, msMax);
    printf("steps longer than the value read before   %8d of %d\n", longer, steps);
    if (longer)
    {
        printf("FAIL: the engine takes steps longer than the routing step it reports\n");
        return 1;
    }
    printf("PASS: the reported routing step is never shorter than the next step taken\n");
    return 0;
}
