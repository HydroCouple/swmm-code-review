/*
 * CON-15 for 6.0.0: KINWAVE books water that never entered when an inflow
 * starts abruptly in an empty conduit (KWSolver::solveConduit copies
 * legacy's no-flow clamp in solveContinuity).
 *
 * Correct behaviour, as in CON-15_test.c: the flow routing continuity error
 * is near zero at routing steps of 60, 30, 15 and 5 s (|error| < 0.1 %; the
 * bug gives about -1 %), and the conduit volume after the first step does
 * not exceed the 15 cfs * dt that has entered.
 * swmm_get_routing_continuity_error() returns a fraction (0.001 = 0.1 %).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

#define DECK "CON-15_step-inflow.inp"
#define CASE "CON-15_case6.inp"

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
    int i, rc, nbad = 0, nruns = 0;
    double worst = 0.0;
    FILE *f = fopen(DECK, "r");
    size_t n = fread(deck, 1, sizeof(deck) - 1, f);
    deck[n] = '\0';
    fclose(f);

    printf("Step (s)  Inflow in 1st step (ft3)  C1 volume after it (ft3)  Continuity error (%%)\n");
    for (i = 0; i < 4; i++)
    {
        double t = 0.0, v1 = -1.0, flowErr = 0.0;
        int first = 1;
        SWMM_Engine e = swmm_engine_create();
        write_case(steps[i]);
        rc = swmm_engine_open(e, CASE, "CON-15_6.rpt", "CON-15_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 0);
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
            if (first) swmm_link_get_volume(e, 0, &v1);
            first = 0;
        }
        if (!rc) rc = swmm_engine_end(e);
        swmm_get_routing_continuity_error(e, &flowErr);
        flowErr *= 100.0;
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        nruns++;
        printf("%8d  %24.1f  %24.1f  %20.3f\n", steps[i], 15.0 * steps[i], v1, flowErr);
        if (rc || fabs(flowErr) >= 0.1 || v1 > 15.0 * steps[i] * 1.001)
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
