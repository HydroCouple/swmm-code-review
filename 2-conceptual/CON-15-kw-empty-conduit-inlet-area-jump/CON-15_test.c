/*
 * CON-15: KINWAVE books water that never entered when an inflow starts
 * abruptly in an empty conduit.
 *
 * kinwave_execute() sets the inlet area at once to the normal-flow area of
 * the new inflow. For a long empty conduit the finite-difference continuity
 * equation then has no root with aout >= 0 (the inlet-area term alone needs
 * far more water than one step supplies); solveContinuity() clamps aout = 0
 * and returns without satisfying continuity, and the conduit is booked with
 * 0.5 * (a1 + a2) * length of water. The surplus never leaves the books, so
 * the run ends with a negative continuity error that does not shrink with
 * the time step.
 *
 * Correct behaviour (conservation of volume): the flow routing continuity
 * error is near zero at every routing step size. The test rewrites
 * ROUTING_STEP in CON-15_step-inflow.inp (60, 30, 15 and 5 s) and requires
 * |error| < 0.1 % (the bug gives about -1 %). With 5.3.0's toolkit header it
 * also reads the conduit volume after the first step, which cannot exceed
 * the 15 cfs * dt that has entered.
 *
 * Steps of 1-2 s are left out: there the Newton tolerance of the KW solver
 * (0.001 of the full area per step) adds a separate drift of +0.1 to +0.2 %
 * whether or not this defect is fixed (see the README).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define DECK "CON-15_step-inflow.inp"
#define CASE "CON-15_case.inp"

static char deck[8192];

static void write_case(int step)
{
    FILE *f = fopen(CASE, "w");
    char *line = deck, *eol;
    while (*line)
    {
        eol = strchr(line, '\n');
        if (!eol) eol = line + strlen(line);
        if (strncmp(line, "ROUTING_STEP ", 13) == 0)
            fprintf(f, "ROUTING_STEP         %d\n", step);
        else
            fprintf(f, "%.*s\n", (int)(eol - line), line);
        line = *eol ? eol + 1 : eol;
    }
    fclose(f);
}

int main(void)
{
    static const int steps[] = {60, 30, 15, 5};
    int i, err, nbad = 0, nruns = 0;
    double worst = 0.0;
    FILE *f = fopen(DECK, "r");
    size_t n = fread(deck, 1, sizeof(deck) - 1, f);
    deck[n] = '\0';
    fclose(f);

    printf("Step (s)  Inflow in 1st step (ft3)  C1 volume after it (ft3)  Continuity error (%%)\n");
    for (i = 0; i < 4; i++)
    {
        double t = 0.0, v1 = -1.0;
        float runoffErr = 0.0f, flowErr = 0.0f, qualErr = 0.0f;
        int first = 1;
        write_case(steps[i]);
        err = swmm_open(CASE, "CON-15.rpt", "CON-15.out");
        if (!err) err = swmm_start(0);
        while (!err)
        {
            err = swmm_step(&t);
            if (t <= 0.0) break;
#ifdef OPENSWMM_LEGACY_SOLVER_H_
            if (first) v1 = swmm_getValue(swmm_LINK_VOLUME, 0);
#endif
            first = 0;
        }
        swmm_end();
        swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
        swmm_close();
        nruns++;
        if (v1 >= 0.0)
            printf("%8d  %24.1f  %24.1f  %20.3f\n", steps[i], 15.0 * steps[i], v1, flowErr);
        else
            printf("%8d  %24.1f  %24s  %20.3f\n", steps[i], 15.0 * steps[i], "n/a", flowErr);
        if (err || fabs(flowErr) >= 0.1 || v1 > 15.0 * steps[i] * 1.001)
        {
            nbad++;
            if (fabs(flowErr) > fabs(worst)) worst = flowErr;
        }
    }
    if (nbad)
    {
        printf("FAIL: %d of %d runs do not conserve volume (worst continuity error %.3f %%)\n",
               nbad, nruns, worst);
        return 1;
    }
    printf("PASS: the conduit holds only the water that entered; continuity error below 0.1 %% "
           "at every step size\n");
    return 0;
}
