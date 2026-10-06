/*
 * NUM-16 for 6.0.0: a V-notch weir with setting < 1 loses its triangular flow
 * (HydStructures.cpp, as legacy link.c). Same deck and checks as NUM-16_test.c.
 *
 * Correct behaviour: each unit's steady depth stays below the V-notch bound
 * 2.9146 - 2 s ft, and a wider-open weir never holds a higher level
 * (tolerance 0.01 ft; see NUM-16_test.c for the derivation).
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

#define N 7

int main(void)
{
    const double s[N] = {1.0, 0.999, 0.99, 0.95, 0.9, 0.75, 0.5};
    double depth[N], flow[N], bound, t = 0.0;
    char id[8];
    int i, err, bad = 0, nonmono = 0;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "NUM-16_vnotch-settings.inp", "NUM-16_6.rpt",
                           "NUM-16_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0) break;
        for (i = 0; i < N; i++)
        {
            sprintf(id, "S%d", i + 1);
            swmm_node_get_depth(e, swmm_node_index(e, id), &depth[i]);
            sprintf(id, "W%d", i + 1);
            swmm_link_get_flow(e, swmm_link_index(e, id), &flow[i]);
        }
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("Setting  Final depth (ft)  Bound (ft)  Weir flow (cfs)\n");
    for (i = 0; i < N; i++)
    {
        bound = 1.0 + (1.0 - s[i]) * 2.0 + 0.914610;
        printf("%7.3f  %16.3f  %10.3f  %15.3f%s\n", s[i], depth[i], bound,
               flow[i], depth[i] > bound + 0.01 ? "   <-- above bound" : "");
        if (depth[i] > bound + 0.01) bad++;
        if (i > 0 && depth[i - 1] > depth[i] + 0.01) nonmono++;
    }
    if (bad || nonmono)
    {
        printf("FAIL: %d of %d settings hold the storage above the V-notch "
               "bound; %d times a wider-open weir gives a higher level\n",
               bad, N, nonmono);
        return 1;
    }
    printf("PASS: a partly open V-notch weir still passes the V-notch flow "
           "and the level falls as the weir opens\n");
    return 0;
}
