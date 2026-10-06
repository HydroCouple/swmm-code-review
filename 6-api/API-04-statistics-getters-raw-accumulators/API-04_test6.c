/*
 * API-04 for 6.0.0: the swmm_stat_* getters (openswmm_statistics.h) copy the
 * internal accumulators, although the header gives their units as project
 * length, flow and volume units and hours. swmm_node_get_stat_time_flooded()
 * and swmm_link_get_stat_surcharge_time() document hours as well.
 *
 * Each statistic is checked against its definition, from values polled after
 * every step with the per-object getters (which return project units):
 *   US deck: J1 time flooded (hours) = sum of steps with overflow > 0, in
 *            hours, through swmm_stat_node_time_flooded() and
 *            swmm_node_get_stat_time_flooded(); C1 surcharge time through
 *            swmm_stat_link_surcharge_time() and
 *            swmm_link_get_stat_surcharge_time() must lie between 0 and the
 *            6-hour duration.
 *   SI deck: J1 max depth (m) = largest polled depth, also through
 *            swmm_stat_node_max_depth_bulk(); J1 volume flooded (m3) = sum of
 *            overflow x step; C2 volume conveyed (m3) = sum of |flow| x step;
 *            S1 precipitation volume = 9.375 mm x 4 ha = 375 m3.
 * Equalities must hold to a relative 1e-3; the defect gives seconds for hours
 * (factor 3600), feet for metres (3.28) and ft3 for m3 (35.3).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_subcatchments.h"
#include "openswmm/engine/openswmm_statistics.h"

static int nbad = 0;

static void check(const char *name, double api, double expected, const char *unit)
{
    int ok = fabs(api - expected) <= 1.0e-3 * fabs(expected) + 1.0e-9;
    printf("  %-42s %12.6g %12.6g  %-5s %s\n", name, api, expected, unit, ok ? "ok" : "WRONG");
    if (!ok) nbad++;
}

static void checkRange(const char *name, double api, double lo, double hi, const char *unit)
{
    int ok = api > lo && api <= hi;
    printf("  %-42s %12.6g %5.0f to %-4.0f %-5s %s\n", name, api, lo, hi, unit, ok ? "ok" : "WRONG");
    if (!ok) nbad++;
}

static int runDeck(const char *inp, int si)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, tPrev = 0.0, dt, v = 0.0, x, bulk[8];
    double maxDepth = 0.0, tFlood = 0.0, vFlood = 0.0, vC2 = 0.0;
    int j1, c1, c2, s1, nn, rc;

    rc = swmm_engine_open(e, inp, si ? "API-04_6si.rpt" : "API-04_6us.rpt",
                          si ? "API-04_6si.out" : "API-04_6us.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    if (rc) { printf("open/start error %d\n", rc); return 1; }
    j1 = swmm_node_index(e, "J1");
    c1 = swmm_link_index(e, "C1");
    c2 = swmm_link_index(e, "C2");
    s1 = swmm_subcatch_index(e, "S1");
    nn = swmm_node_count(e);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        dt = (t - tPrev) * 86400.0;
        tPrev = t;
        swmm_node_get_depth(e, j1, &v);
        if (v > maxDepth) maxDepth = v;
        swmm_node_get_overflow(e, j1, &v);
        if (v > 0.0) { tFlood += dt; vFlood += v * dt; }
        swmm_link_get_flow(e, c2, &v);
        vC2 += fabs(v) * dt;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (rc) { printf("run error %d\n", rc); return 1; }

    printf("%s (%s units)\n", inp, si ? "SI" : "US");
    printf("  %-42s %12s %12s\n", "statistic", "getter", "expected");
    if (!si)
    {
        swmm_stat_node_time_flooded(e, j1, &x);
        check("swmm_stat_node_time_flooded J1", x, tFlood / 3600.0, "hours");
        swmm_node_get_stat_time_flooded(e, j1, &x);
        check("swmm_node_get_stat_time_flooded J1", x, tFlood / 3600.0, "hours");
        swmm_stat_link_surcharge_time(e, c1, &x);
        checkRange("swmm_stat_link_surcharge_time C1", x, 0.0, 6.0, "hours");
        swmm_link_get_stat_surcharge_time(e, c1, &x);
        checkRange("swmm_link_get_stat_surcharge_time C1", x, 0.0, 6.0, "hours");
    }
    else
    {
        swmm_stat_node_max_depth(e, j1, &x);
        check("swmm_stat_node_max_depth J1", x, maxDepth, "m");
        if (nn <= 8 && swmm_stat_node_max_depth_bulk(e, bulk, nn) == 0)
            check("swmm_stat_node_max_depth_bulk J1", bulk[j1], maxDepth, "m");
        swmm_stat_node_vol_flooded(e, j1, &x);
        check("swmm_stat_node_vol_flooded J1", x, vFlood, "m3");
        swmm_stat_link_vol_flow(e, c2, &x);
        check("swmm_stat_link_vol_flow C2", x, vC2, "m3");
        swmm_stat_subcatch_precip(e, s1, &x);
        check("swmm_stat_subcatch_precip S1", x, 375.0, "m3");
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return 0;
}

int main(void)
{
    if (runDeck("API-04_us.inp", 0) || runDeck("API-04_si.inp", 1)) return 1;
    if (nbad)
    {
        printf("FAIL: %d of 9 statistics are not in the documented units "
               "(seconds for hours, ft/ft3 in an SI project)\n", nbad);
        return 1;
    }
    printf("PASS: every statistic checked is in its documented unit and matches its definition\n");
    return 0;
}
