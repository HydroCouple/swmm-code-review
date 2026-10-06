/*
 * API-15 for 6.0.0: a rain gage scale factor changed during a run with
 * swmm_gage_set_scale_factor() must take effect on the next step on its own
 * gage only (the header says "the new value takes effect on the next
 * timestep").
 *
 * Same deck and check as API-15_test.c: G1 and G2 share TS1 (1.0 in/hr from
 * 0:00 to 5:00); at 2:30 the test sets G1's scale factor to 2.0. Expected:
 * S1 2.0 in/hr from the next runoff step to 5:00 (7.50 in in total), S2
 * 1.0 in/hr throughout (5.00 in). swmm_subcatch_get_rainfall returns in/hr.
 * The first 5 minutes after the change are not checked. Rates must match
 * within 0.001 in/hr, totals within 0.02 in.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_gages.h"
#include "openswmm/engine/openswmm_subcatchments.h"

#define RPT "API-15_6.rpt"

static int readTotalPrecip(double precip[2])
{
    char line[512], name[32];
    double p;
    int found = 0, inTable = 0;
    FILE *f = fopen(RPT, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f) && found < 2)
    {
        if (strstr(line, "Subcatchment Runoff Summary")) inTable = 1;
        if (!inTable) continue;
        if (sscanf(line, " %31s %lf", name, &p) == 2 && name[0] == 'S'
            && (name[1] == '1' || name[1] == '2') && name[2] == '\0')
        {
            precip[name[1] - '1'] = p;
            found++;
        }
    }
    fclose(f);
    return found == 2;
}

int main(void)
{
    const double tSet = 2.5 / 24.0, tSkip = 5.0 / 1440.0, tEnd = 5.0 / 24.0;
    double t = 0.0, r[2], e[2], precip[2] = {-1.0, -1.0};
    int err, g1, set = 0, nbad = 0, ok = 1;
    SWMM_Engine eng = swmm_engine_create();

    err = swmm_engine_open(eng, "API-15_shared-series.inp", RPT, "API-15_6.out", NULL);
    if (!err) err = swmm_engine_initialize(eng);
    if (!err) err = swmm_engine_start(eng, 1);
    g1 = swmm_gage_index(eng, "G1");
    printf("time    S1 rain  expected   S2 rain  expected   (in/hr)\n");
    while (!err)
    {
        if (!set && t >= tSet - 1.0e-9)
        {
            swmm_gage_set_scale_factor(eng, g1, 2.0);
            set = 1;
            printf("-- scale factor of G1 set to 2.0 at %.2f h\n", t * 24.0);
        }
        err = swmm_engine_step(eng, &t);
        if (t <= 0.0) break;
        if (t > tEnd + 1.0e-9) continue;              /* rain has ended */
        r[0] = r[1] = 0.0;
        swmm_subcatch_get_rainfall(eng, 0, &r[0]);
        swmm_subcatch_get_rainfall(eng, 1, &r[1]);
        e[0] = (t <= tSet + 1.0e-9) ? 1.0 : 2.0;
        e[1] = 1.0;
        /* print every half hour */
        if (fabs(t * 48.0 - floor(t * 48.0 + 0.5)) < 1.0e-6)
            printf("%4.1f h   %6.3f   %6.3f     %6.3f   %6.3f\n",
                   t * 24.0, r[0], e[0], r[1], e[1]);
        if (t > tSet + 1.0e-9 && t < tSet + tSkip - 1.0e-9) continue;
        if (fabs(r[0] - e[0]) > 0.001 || fabs(r[1] - e[1]) > 0.001) nbad++;
    }
    if (!err) err = swmm_engine_end(eng);
    if (!err) err = swmm_engine_report(eng);
    swmm_engine_close(eng);
    swmm_engine_destroy(eng);
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    if (!readTotalPrecip(precip))
    {
        printf("FAIL: could not read the Subcatchment Runoff Summary\n");
        return 1;
    }
    printf("Total Precip (in): S1 %.2f (expected 7.50), S2 %.2f (expected 5.00)\n",
           precip[0], precip[1]);
    printf("steps with a wrong rate: %d\n", nbad);
    if (nbad || fabs(precip[0] - 7.5) > 0.02 || fabs(precip[1] - 5.0) > 0.02) ok = 0;

    if (!ok)
    {
        printf("FAIL: the new scale factor of G1 is applied late (S1 %.2f in instead of "
               "7.50) and changes G2 (S2 %.2f in instead of 5.00); %d steps wrong\n",
               precip[0], precip[1], nbad);
        return 1;
    }
    printf("PASS: G1's new scale factor applies from the next step and G2 is "
           "unchanged (7.50 and 5.00 in)\n");
    return 0;
}
