/*
 * NUM-24 for 6.0.0: a transect and its mirror image must have the same
 * hydraulics.
 *
 * Same deck and checks as NUM-24_test.c: T1 and T2 are the same channel
 * reflected left to right (n = 0.03), with the lower end extended by a
 * vertical wall to the full height. Full flows must agree to 0.5 % and the
 * steady depths at 60 cfs to 1 % (the defect: 19 % and 4 %), and both full
 * flows must be within 1 % of the reference manual's algorithm, which counts
 * the wall's wetted perimeter: 1.486/0.03 * 44 * (44/17.62)^(2/3) * sqrt(S).
 * The full flows are read from the report's Cross Section Summary, since the
 * 6.0.0 API has no full-flow getter.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

static double fullFlow(const char *rpt, const char *link)
{
    char line[256], name[64];
    double v[6], q = -1.0;
    int in = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Cross Section Summary")) in = 1;
        else if (in && sscanf(line, " %63s %*s %lf %lf %lf %lf %lf %lf",
                              name, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 7
                 && strcmp(name, link) == 0) { q = v[5]; break; }
    }
    fclose(f);
    return q;
}

int main(void)
{
    double qfull[2], depth[2] = {0}, slope = 0, t = 0, a, p, expect;
    int i, rc, nd[2];
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-24_mirror.inp", "NUM-24_6.rpt", "NUM-24_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    nd[0] = swmm_node_index(e, "J1");
    nd[1] = swmm_node_index(e, "J2");
    if (!rc) rc = swmm_link_get_slope(e, swmm_link_index(e, "C1"), &slope);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        for (i = 0; i < 2; i++) swmm_node_get_depth(e, nd[i], &depth[i]);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

    qfull[0] = fullFlow("NUM-24_6.rpt", "C1");
    qfull[1] = fullFlow("NUM-24_6.rpt", "C2");
    a = 44.0;
    p = 4.0 + sqrt(5.0) + 6.0 + sqrt(29.0);
    expect = 1.486 / 0.03 * a * pow(a / p, 2.0 / 3.0) * sqrt(slope);

    printf("Conduit  Transect                       Full flow (cfs)  Depth at 60 cfs (ft)\n");
    printf("C1       T1 low end on the left             %8.2f          %6.3f\n", qfull[0], depth[0]);
    printf("C2       T2 low end on the right            %8.2f          %6.3f\n", qfull[1], depth[1]);
    printf("Full flow with both end walls in the wetted perimeter: %.2f cfs\n", expect);

    if (fabs(qfull[1] / qfull[0] - 1.0) > 0.005 || fabs(depth[1] / depth[0] - 1.0) > 0.01 ||
        fabs(qfull[0] / expect - 1.0) > 0.01 || fabs(qfull[1] / expect - 1.0) > 0.01)
    {
        printf("FAIL: the mirror image has %+.1f %% full flow (%.2f vs %.2f cfs) and "
               "%+.1f %% depth at 60 cfs\n", 100.0 * (qfull[1] / qfull[0] - 1.0),
               qfull[1], qfull[0], 100.0 * (depth[1] / depth[0] - 1.0));
        return 1;
    }
    printf("PASS: a transect and its mirror image have the same full flow and depth\n");
    return 0;
}
