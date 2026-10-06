/*
 * API-15: a rain gage scale factor changed during a run with
 * swmm_setValue(swmm_GAGE_SCALEFACTOR) takes effect late on its own gage and
 * at once on the other gages that share its time series.
 *
 * G1 and G2 read the same series TS1: hourly records of 1.0 in/hr from 0:00
 * to 5:00. Subcatchments S1 (on G1) and S2 (on G2) are identical. At 2:30,
 * in the middle of a record, the test sets G1's scale factor to 2.0 and
 * leaves G2's at 1.0. The scale factor multiplies the gage's rainfall, so
 * from the next runoff step on:
 *   S1 receives 2.0 in/hr until 5:00 (total 2.5 x 1.0 + 2.5 x 2.0 = 7.50 in);
 *   S2 keeps receiving 1.0 in/hr (total 5.00 in).
 * The test reads the subcatchment rainfall (in/hr) after every step and the
 * Total Precip of each subcatchment from the report's Subcatchment Runoff
 * Summary. The first 5 minutes after the change (one runoff step) are not
 * checked. Rates must match within 0.001 in/hr (the wrong ones are off by
 * 0.5 or 1.0 in/hr), totals within 0.02 in (the wrong ones are off by at
 * least 0.75 in).
 *
 * swmm_GAGE_SCALEFACTOR does not exist in 5.2.4 (no gage scale factor).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
#define RPT "API-15.rpt"

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
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    const double tSet = 2.5 / 24.0, tSkip = 5.0 / 1440.0, tEnd = 5.0 / 24.0;
    double t = 0.0, r[2], e[2], precip[2] = {-1.0, -1.0};
    int err, g1, set = 0, nbad = 0, ok = 1;

    err = swmm_open("API-15_shared-series.inp", RPT, "API-15.out");
    if (!err) err = swmm_start(1);
    g1 = swmm_getIndex(swmm_GAGE, "G1");
    printf("time    S1 rain  expected   S2 rain  expected   (in/hr)\n");
    while (!err)
    {
        if (!set && t >= tSet - 1.0e-9)
        {
            swmm_setValue(swmm_GAGE_SCALEFACTOR, g1, 2.0);
            set = 1;
            printf("-- scale factor of G1 set to 2.0 at %.2f h\n", t * 24.0);
        }
        err = swmm_step(&t);
        if (t <= 0.0) break;
        if (t > tEnd + 1.0e-9) continue;              /* rain has ended */
        r[0] = swmm_getValue(swmm_SUBCATCH_RAINFALL, 0);
        r[1] = swmm_getValue(swmm_SUBCATCH_RAINFALL, 1);
        e[0] = (t <= tSet + 1.0e-9) ? 1.0 : 2.0;
        e[1] = 1.0;
        /* print every half hour */
        if (fabs(t * 48.0 - floor(t * 48.0 + 0.5)) < 1.0e-6)
            printf("%4.1f h   %6.3f   %6.3f     %6.3f   %6.3f\n",
                   t * 24.0, r[0], e[0], r[1], e[1]);
        if (t > tSet + 1.0e-9 && t < tSet + tSkip - 1.0e-9) continue;
        if (fabs(r[0] - e[0]) > 0.001 || fabs(r[1] - e[1]) > 0.001) nbad++;
    }
    swmm_end();
    swmm_report();
    swmm_close();
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
#else
    printf("PASS: not affected, 5.2.4 has no gage scale factor property\n");
    return 0;
#endif
}
