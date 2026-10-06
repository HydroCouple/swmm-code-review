/*
 * NUM-22 for 6.0.0: an NC line of zeros must reuse the n values as written.
 *
 * Same deck and checks as NUM-22_test.c: T2 ("NC 0 0 0" after the meandering
 * T1) must have the full flow of the analytic subsection sum with
 * n = 0.05 / 0.05 / 0.03, 538.4 cfs at S = 0.001 (tolerance 2 %), and with
 * 20 cfs its steady depth must match T3, the same transect with the n values
 * written out (tolerance 1 %). The full flows are read from the report's
 * Cross Section Summary, since the 6.0.0 API has no full-flow getter.
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
    const char *nodes[3] = {"J1", "J2", "J3"};
    double depth[3] = {0}, qfull[3], slope = 0, t = 0, expect, sumk;
    int i, rc, nd[3];
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-22_transects.inp", "NUM-22_6.rpt", "NUM-22_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    for (i = 0; i < 3; i++) nd[i] = swmm_node_index(e, nodes[i]);
    if (!rc) rc = swmm_link_get_slope(e, swmm_link_index(e, "C2"), &slope);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        for (i = 0; i < 3; i++) swmm_node_get_depth(e, nd[i], &depth[i]);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

    qfull[0] = fullFlow("NUM-22_6.rpt", "C1");
    qfull[1] = fullFlow("NUM-22_6.rpt", "C2");
    qfull[2] = fullFlow("NUM-22_6.rpt", "C3");

    sumk = 2.0 * 1.486 / 0.05 * 25.0 * pow(25.0 / sqrt(125.0), 2.0 / 3.0)
         + 1.486 / 0.03 * 100.0 * pow(100.0 / 20.0, 2.0 / 3.0);
    expect = sumk * sqrt(slope);

    printf("Conduit  Transect                 Full flow (cfs)  Depth at 20 cfs (ft)\n");
    printf("C1       T1 meander 2.0              %8.2f          %6.3f\n", qfull[0], depth[0]);
    printf("C2       T2 NC 0 0 0 (inherits)      %8.2f          %6.3f\n", qfull[1], depth[1]);
    printf("C3       T3 NC 0.05 0.05 0.03        %8.2f          %6.3f\n", qfull[2], depth[2]);
    printf("Analytic full flow with n = 0.05/0.05/0.03: %.2f cfs\n", expect);

    if (fabs(qfull[1] / expect - 1.0) > 0.02 || fabs(qfull[2] / expect - 1.0) > 0.02 ||
        fabs(depth[1] / depth[2] - 1.0) > 0.01)
    {
        printf("FAIL: T2 (NC 0 0 0) has full flow %.2f cfs (%+.0f %% from %.2f) and depth "
               "%.3f ft vs %.3f ft for the same n written out\n", qfull[1],
               100.0 * (qfull[1] / expect - 1.0), expect, depth[1], depth[2]);
        return 1;
    }
    printf("PASS: NC 0 0 0 reuses the n values as written, not the meander-adjusted ones\n");
    return 0;
}
