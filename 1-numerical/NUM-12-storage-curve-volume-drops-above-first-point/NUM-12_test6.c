/*
 * NUM-12 for 6.0.0: table_getStorageVolume() (data/TableData.hpp) drops the
 * volume below the first point of a storage curve once the depth passes it.
 *
 * Same deck and checks as NUM-12_test.c: both storage units must hold the
 * 3600 ft3 that entered (within 1%) at 4.1 ft (within 0.01 ft), and the
 * stored volume must never drop by more than 1 ft3 while they fill.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    const char *ids[2] = {"SU1", "SU2"};
    double t = 0.0, v[2] = {0, 0}, d[2] = {0, 0}, vprev[2] = {0, 0};
    double drop[2] = {0, 0}, dropAt[2] = {0, 0}, vin = 0.5 * 7200.0;
    double flowErr = 0.0;
    int i, k[2], err, bad = 0;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "NUM-12_storage-fill.inp", "NUM-12_6.rpt",
                           "NUM-12_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    for (i = 0; i < 2; i++) k[i] = swmm_node_index(e, ids[i]);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (err) break;
        for (i = 0; i < 2; i++)
        {
            swmm_node_get_volume(e, k[i], &v[i]);
            swmm_node_get_depth(e, k[i], &d[i]);
            if (vprev[i] - v[i] > drop[i])
            {
                drop[i] = vprev[i] - v[i];
                dropAt[i] = d[i];
            }
            vprev[i] = v[i];
        }
        if (t <= 0) break;    /* the call that ends the run still steps */
    }
    if (!err) err = swmm_engine_end(e);
    swmm_get_routing_continuity_error(e, &flowErr);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
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
    printf("Flow routing continuity error: %.3f %%\n", 100.0 * flowErr);

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
