/*
 * API-07: 5.3.0's swmm_SUBCATCH_POLLUTANT_BUILDUP getter with several land
 * uses.
 *
 * S1 is 10 ac: L1 20% (2 ac), L2 30% (3 ac), L3 50% (5 ac). After DRY_DAYS 5
 * the EXP buildup (rate 10/day, so 1 - exp(-50) = 1) is at its maximum:
 * 50 lb/ac on L1 = 100 lb, 10 lb/ac on L2 = 30 lb, nothing on L3. The
 * subcatchment carries 130 lb (the report's Initial Buildup), i.e. 13 lb/ac.
 * The getter must return 13 lb/ac (checked within 1%). The defect adds the
 * three land uses' own densities: 50 + 10 + 0 = 60 lb/ac. (An unweighted
 * mean of the densities would give 20.)
 *
 * The value is read after swmm_start and again after the first (dry) step;
 * the buildup is at its maximum, so it does not change.
 *
 * 5.2.4 has no expanded getters and is not affected.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    double elapsed = 0.0, b0 = 0.0, b1 = 0.0;
    int err, s1;

    err = swmm_open("API-07_three-land-uses.inp", "API-07.rpt", "API-07.out");
    if (!err) err = swmm_start(1);
    s1 = swmm_getIndex(swmm_SUBCATCH, "S1");
    b0 = swmm_getValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_POLLUTANT_BUILDUP, s1, 0, 0);
    if (!err) err = swmm_step(&elapsed);
    b1 = swmm_getValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_POLLUTANT_BUILDUP, s1, 0, 0);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_report();
    swmm_close();

    printf("S1: L1 2 ac x 50 lb/ac + L2 3 ac x 10 lb/ac + L3 5 ac x 0 = 130 lb on 10 ac\n");
    printf("%-34s %12s %12s\n", "SUBCATCH_POLLUTANT_BUILDUP(S1, P1)", "API (lb/ac)", "expected");
    printf("%-34s %12.4f %12.4f\n", "after swmm_start", b0, 13.0);
    printf("%-34s %12.4f %12.4f\n", "after the first step", b1, 13.0);
    if (err) { printf("FAIL: run error %d\n", err); return 1; }
    if (fabs(b0 - 13.0) > 0.13 || fabs(b1 - 13.0) > 0.13)
    {
        printf("FAIL: the buildup getter returns %.4f lb/ac for 130 lb on 10 ac (13 lb/ac)\n", b1);
        return 1;
    }
    printf("PASS: the buildup getter returns the subcatchment's buildup per unit area "
           "(13 lb/ac)\n");
    return 0;
#else
    printf("5.2.4 has no swmm_getValueExpanded and no buildup property\n");
    printf("PASS: not affected (the API does not exist in 5.2.4)\n");
    return 0;
#endif
}
