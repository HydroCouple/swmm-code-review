/*
 * IO-11: the Barrels item of an IRREGULAR cross section is ignored.
 *
 * link_readXsectParams() returns right after it has found the transect of an
 * IRREGULAR line, before the code that reads Barrels, so the conduit keeps
 * the default of 1 barrel. The GUI writes IRREGULAR lines as
 * "Link IRREGULAR Tsect 0 0 0 Barrels".
 *
 * IO-11_irregular-barrels.inp has two identical channels on the same transect:
 *   C1  2 barrels, 20 cfs
 *   C2  1 barrel,  10 cfs
 * With 2 barrels each barrel of C1 carries 10 cfs, the same as C2's single
 * barrel, so at steady state both inlet junctions must stand at the same
 * depth.
 *
 * Correct behaviour: the report's Cross Section Summary shows 2 barrels for
 * C1, and the depths at J1 and J2 at the end of the 2-hour run agree within
 * 0.005 ft. (Ignoring the second barrel puts 20 cfs through one channel,
 * which raises J1 by about 0.3 ft.)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* number of barrels of C1 and C2 from the Cross Section Summary */
static int readBarrels(const char *rpt, int nb[2])
{
    FILE *f = fopen(rpt, "r");
    char line[512], name[64], shape[64];
    double d, a, r, w;
    int n, in = 0, found = 0;
    if (!f) return 0;
    while (found < 2 && fgets(line, sizeof line, f))
    {
        if (strstr(line, "Cross Section Summary")) { in = 1; continue; }
        if (in && sscanf(line, "%63s %63s %lf %lf %lf %lf %d", name, shape, &d, &a, &r, &w, &n) == 7)
        {
            if (strcmp(name, "C1") == 0) { nb[0] = n; found++; }
            if (strcmp(name, "C2") == 0) { nb[1] = n; found++; }
        }
    }
    fclose(f);
    return found;
}

int main(void)
{
    double y1 = 0.0, y2 = 0.0, q1 = 0.0, q2 = 0.0;
    int err, nb[2] = {0, 0};

    err = swmm_open("IO-11_irregular-barrels.inp", "IO-11.rpt", "IO-11.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        double t = 0.0;
        err = swmm_step(&t);
        if (t <= 0.0) break;
        y1 = swmm_getValue(swmm_NODE_DEPTH, swmm_getIndex(swmm_NODE, "J1"));
        y2 = swmm_getValue(swmm_NODE_DEPTH, swmm_getIndex(swmm_NODE, "J2"));
        q1 = swmm_getValue(swmm_LINK_FLOW, swmm_getIndex(swmm_LINK, "C1"));
        q2 = swmm_getValue(swmm_LINK_FLOW, swmm_getIndex(swmm_LINK, "C2"));
    }
    swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    readBarrels("IO-11.rpt", nb);

    printf("Link  Barrels  Barrels in  Flow    Flow per      Inlet depth\n");
    printf("      entered  report      (cfs)   barrel (cfs)  at end (ft)\n");
    printf("C1    %7d  %10d  %6.2f  %12.2f  %11.3f\n", 2, nb[0], q1, nb[0] ? q1 / nb[0] : 0.0, y1);
    printf("C2    %7d  %10d  %6.2f  %12.2f  %11.3f\n", 1, nb[1], q2, nb[1] ? q2 / nb[1] : 0.0, y2);

    if (nb[0] != 2 || nb[1] != 1 || fabs(y1 - y2) > 0.005)
    {
        printf("FAIL: C1 was given 2 barrels but has %d; J1 stands at %.3f ft instead of "
               "%.3f ft (the depth of one barrel carrying 10 cfs)\n", nb[0], y1, y2);
        return 1;
    }
    printf("PASS: C1 has 2 barrels and each carries the same flow at the same depth as C2\n");
    return 0;
}
