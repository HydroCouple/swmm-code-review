/*
 * NUM-23 for 6.0.0: in SI units with a width factor, the bank stations are
 * not found.
 *
 * Same deck and checks as NUM-23_test.c: T1 (stations 0/10/30/40 m, banks
 * 10/30, Wfactor 1.5) must have the analytic full flow of the scaled section,
 * sqrt(S) * sum(1/n_i * A_i * R_i^(2/3)) = 109.8 m3/s at S = 0.001
 * (tolerance 2 %; the defect gives -66 %), and its steady depth at 5 m3/s
 * must match T2, the same section with the stations entered pre-multiplied
 * (tolerance 1 %). The full flows are read from the report's Cross Section
 * Summary, since the 6.0.0 API has no full-flow getter.
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
    double qfull[2], depth[2] = {0}, slope = 0, t = 0, expect;
    int i, rc, nd[2];
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-23_si-wfactor.inp", "NUM-23_6.rpt", "NUM-23_6.out", NULL);
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

    qfull[0] = fullFlow("NUM-23_6.rpt", "C1");
    qfull[1] = fullFlow("NUM-23_6.rpt", "C2");
    expect = sqrt(slope) * (2.0 / 0.10 * 15.0 * pow(15.0 / sqrt(229.0), 2.0 / 3.0)
                            + 1.0 / 0.03 * 60.0 * pow(60.0 / 30.0, 2.0 / 3.0));

    printf("Conduit  Transect                       Full flow (m3/s)  Depth at 5 m3/s (m)\n");
    printf("C1       T1 0/10/30/40 m, Wfactor 1.5       %8.2f           %6.3f\n", qfull[0], depth[0]);
    printf("C2       T2 0/15/45/60 m                    %8.2f           %6.3f\n", qfull[1], depth[1]);
    printf("Analytic full flow: %.2f m3/s\n", expect);

    if (fabs(qfull[0] / expect - 1.0) > 0.02 || fabs(qfull[1] / expect - 1.0) > 0.02 ||
        fabs(depth[0] / depth[1] - 1.0) > 0.01)
    {
        printf("FAIL: with Wfactor 1.5 the full flow is %.2f m3/s (%+.0f %% from %.2f) and the "
               "depth %.3f m vs %.3f m for the same stations entered directly\n", qfull[0],
               100.0 * (qfull[0] / expect - 1.0), expect, depth[0], depth[1]);
        return 1;
    }
    printf("PASS: the bank stations are found and Wfactor gives the same section as scaled stations\n");
    return 0;
}
