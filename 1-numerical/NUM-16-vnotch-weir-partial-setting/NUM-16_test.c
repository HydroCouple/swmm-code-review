/*
 * NUM-16: a V-notch weir with setting < 1 loses its triangular flow.
 *
 * Seven identical storage units (1000 ft2) each receive 2 cfs and drain over
 * a 90-degree V-notch weir (TRIANGULAR 2 ft x 4 ft, side slope 1, Cd 2.5,
 * crest 1 ft). Control rules fix the weir settings at 1, 0.999, 0.99, 0.95,
 * 0.9, 0.75 and 0.5. After 12 hours every unit is at steady state and its
 * weir passes the 2 cfs inflow.
 *
 * A setting s raises the weir's crest by (1 - s) * 2 ft. Whatever s is, the
 * opening above the raised crest still contains the full 90-degree notch, so
 * the weir passes at least the V-notch flow Cd * 1 * h^2.5 at head h. Two
 * consequences that do not depend on how the rest of the opening is modelled:
 *   1. steady depth <= 1 + (1 - s) * 2 + (2 / 2.5)^0.4 = 2.9146 - 2 s  ft
 *      (V-notch head for 2 cfs is 0.9146 ft);
 *   2. opening the weir further (larger s) cannot raise the steady depth.
 *
 * Tolerance 0.01 ft on both checks (dynamic-wave steady state is within
 * 0.001 ft of the analytic depth for s = 1). The defect breaks the bound by
 * up to 8 ft (the 0.999 unit fills to its 10 ft maximum and floods).
 */
#include <stdio.h>
#include "swmm5.h"

#define N 7

int main(void)
{
    const double s[N] = {1.0, 0.999, 0.99, 0.95, 0.9, 0.75, 0.5};
    double depth[N], flow[N], bound, t = 0.0;
    char id[8];
    int i, err, bad = 0, nonmono = 0;

    err = swmm_open("NUM-16_vnotch-settings.inp", "NUM-16.rpt", "NUM-16.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        for (i = 0; i < N; i++)
        {
            sprintf(id, "S%d", i + 1);
            depth[i] = swmm_getValue(swmm_NODE_DEPTH, swmm_getIndex(swmm_NODE, id));
            sprintf(id, "W%d", i + 1);
            flow[i] = swmm_getValue(swmm_LINK_FLOW, swmm_getIndex(swmm_LINK, id));
        }
    }
    swmm_end();
    swmm_report();
    swmm_close();
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
