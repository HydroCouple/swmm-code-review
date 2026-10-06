/*
 * NUM-12: table_getStorageVolume() drops the volume below the first point of
 * a storage curve once the depth passes that point.
 *
 * Two storage units receive 0.5 cfs for 2 hours (3600 ft3) and cannot drain
 * (outlet offset 5.5 ft). SU1 uses the curve (1 ft, 1000 ft2)(6 ft, 1000 ft2);
 * SU2 the same curve with (0, 0) written as its first point. Below the first
 * tabulated depth SWMM takes the area to grow linearly from 0 at depth 0, in
 * table_getStorageVolume() and table_getStorageDepth() alike, so the two
 * curves describe the same basin: 500 ft3 below 1 ft, then 1000 ft3 per ft.
 *
 * Correct behaviour, from continuity: the volume stored never decreases while
 * the unit fills, it equals the 3600 ft3 that entered, and the depth is
 * 1 + (3600 - 500) / 1000 = 4.1 ft in both units.
 *
 * Tolerances: volume within 1% (36 ft3) and depth within 0.01 ft; the step
 * from one routing step to the next is 5 ft3, so a drop larger than 1 ft3 is
 * not a rounding effect. The defect loses the 500 ft3 triangle (14% of the
 * volume) at the moment the depth crosses 1 ft.
 */
#include <stdio.h>
#include "swmm5.h"

int main(void)
{
    const char *ids[2] = {"SU1", "SU2"};
    double t = 0.0, v[2] = {0, 0}, d[2] = {0, 0}, vprev[2] = {0, 0};
    double drop[2] = {0, 0}, dropAt[2] = {0, 0}, vin = 0.5 * 7200.0;
    float runoffErr, flowErr, qualErr;
    int i, k[2], err, bad = 0;

    err = swmm_open("NUM-12_storage-fill.inp", "NUM-12.rpt", "NUM-12.out");
    if (!err) err = swmm_start(1);
    for (i = 0; i < 2; i++) k[i] = swmm_getIndex(swmm_NODE, ids[i]);
    while (!err)
    {
        err = swmm_step(&t);
        if (err) break;
        for (i = 0; i < 2; i++)
        {
            v[i] = swmm_getValue(swmm_NODE_VOLUME, k[i]);
            d[i] = swmm_getValue(swmm_NODE_DEPTH, k[i]);
            if (vprev[i] - v[i] > drop[i])
            {
                drop[i] = vprev[i] - v[i];
                dropAt[i] = d[i];
            }
            vprev[i] = v[i];
        }
        if (t <= 0) break;    /* the call that ends the run still steps */
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("Inflow volume: %.0f ft3; expected depth 4.100 ft\n", vin);
    printf("Unit  Final volume (ft3)  Final depth (ft)  Largest volume drop (ft3)\n");
    for (i = 0; i < 2; i++)
    {
        printf("%-4s  %18.1f  %16.3f  %10.1f at depth %.3f ft\n", ids[i], v[i],
               d[i], drop[i], dropAt[i]);
        if (v[i] < 0.99 * vin || v[i] > 1.01 * vin) bad++;
        else if (d[i] < 4.09 || d[i] > 4.11) bad++;
        else if (drop[i] > 1.0) bad++;
    }
    printf("Flow routing continuity error: %.3f %%\n", flowErr);

    if (bad)
    {
        printf("FAIL: %d storage unit(s) do not hold the volume that entered "
               "(SU1 %.1f ft3, SU2 %.1f ft3 of %.0f)\n", bad, v[0], v[1], vin);
        return 1;
    }
    printf("PASS: both units hold the 3600 ft3 that entered at 4.1 ft, and "
           "the volume never drops\n");
    return 0;
}
