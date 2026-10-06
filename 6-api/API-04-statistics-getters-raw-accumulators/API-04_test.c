/*
 * API-04: the 5.3.0 statistics getters (swmm_getNodeStats, swmm_getLinkStats,
 * swmm_getOutfallStats, swmm_getSubcatchStats, swmm_getSystemRunoffTotals,
 * swmm_getSystemRoutingTotals, ...) return the raw accumulators of stats.c
 * and massbal.c instead of the statistics their header documents.
 *
 * Each statistic is checked against its definition, computed by the test
 * from values polled with swmm_getValueExpanded() after every routing step
 * (the statistics are updated once per routing step from the same state):
 *   node avgDepth      "Average node depth"      = mean of the polled depths,
 *                      per step or per unit time (CON-21 changes the engine's
 *                      averages from the first to the second; either is a
 *                      correct average, the raw sum this issue is about is not)
 *   node timeFlooded   "Total time flooded (hours)" = sum of steps with
 *                      overflow > 0, in hours (no ponding in the decks)
 *   node maxDepth      "Maximum node depth"      = largest polled depth (SI deck:
 *                      metres, like every other getter of an SI project)
 *   node volFlooded    "Total volume flooded"    = sum of overflow x step
 *   link timeInFlowClass "Time in each flow class" summed over the classes =
 *                      the routed time in hours (every step adds its length
 *                      to exactly one class)
 *   outfall avgFlow    "Average flow rate"       = mean of the polled inflow
 *                      over the steps with flow (>= 0.001 cfs), as in the report,
 *                      per step or per unit time (see avgDepth)
 *   outfall totalLoad  pollutant load, in the report's unit (lbs) = sum of
 *                      inflow x concentration x step
 *   subcatch precip    "Total precipitation (depth)" = 0.375 in / 9.375 mm
 *                      (the gage series: 15 min at 1.0 in/hr + 15 min at 0.5)
 *   RunoffTotals.rainfall "Total rainfall (depth)" = the same depth
 *   RoutingTotals.outflow "(all in ft3 or m3)"    = sum of outfall inflow x step
 * A check passes within a relative 1e-3 (the polled sums repeat the engine's
 * own sums), the load within 1 % (see below); the defect is off by a factor
 * of 60 or more, except the SI maxDepth (factor 3.28) and SI volumes (35.3).
 *
 * 5.2.4 has no statistics getters, so it is not affected.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
static int nbad = 0;

static void checkTol(const char *name, double api, double expected, const char *unit, double tol)
{
    int ok = fabs(api - expected) <= tol * fabs(expected) + 1.0e-9;
    printf("  %-34s %14.6g %14.6g  %-6s %s\n", name, api, expected, unit, ok ? "ok" : "WRONG");
    if (!ok) nbad++;
}

static void check(const char *name, double api, double expected, const char *unit)
{
    checkTol(name, api, expected, unit, 1.0e-3);
}

/* an average that may be taken per routing step or per unit time */
static void checkMean(const char *name, double api, double perStep, double perTime, const char *unit)
{
    double expected = fabs(api - perTime) < fabs(api - perStep) ? perTime : perStep;
    checkTol(name, api, expected, unit, 1.0e-3);
}

static int runDeck(const char *inp, int si)
{
    double t = 0.0, tNow, tPrev = 0.0, dt, v, c;
    double sumDepth = 0.0, maxDepth = 0.0, tFlood = 0.0, vFlood = 0.0;
    double sumOutQ = 0.0, vOut = 0.0, load = 0.0, routed = 0.0, sumClass = 0.0;
    double sumDepthT = 0.0, sumOutQT = 0.0, tFlow = 0.0;
    long nSteps = 0, nFlow = 0;
    int j1, o1, c1, s1, k, err;
    double outLoad[1] = {0.0};
    swmm_NodeStats ns;
    swmm_LinkStats ls;
    swmm_OutfallStats os;
    swmm_SubcatchStats ss;
    swmm_RunoffTotals rt;
    swmm_RoutingTotals ft;

    err = swmm_open(inp, si ? "API-04_si.rpt" : "API-04_us.rpt", si ? "API-04_si.out" : "API-04_us.out");
    if (!err) err = swmm_start(0);
    if (err) { printf("open/start error %d\n", err); return 1; }
    j1 = swmm_getIndex(swmm_NODE, "J1");
    o1 = swmm_getIndex(swmm_NODE, "O1");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    s1 = swmm_getIndex(swmm_SUBCATCH, "S1");
    while (!err)
    {
        err = swmm_step(&t);
        if (err) break;
        /* the last step returns t = 0; both decks end at 6 h = 0.25 days */
        tNow = (t > 0.0) ? t : 0.25;
        dt = (tNow - tPrev) * 86400.0;
        tPrev = tNow;
        routed += dt;
        nSteps++;
        v = swmm_getValueExpanded(swmm_NODE, swmm_NODE_DEPTH, j1, -1, -1);
        sumDepth += v;
        sumDepthT += v * dt;
        if (v > maxDepth) maxDepth = v;
        v = swmm_getValueExpanded(swmm_NODE, swmm_NODE_OVERFLOW, j1, -1, -1);
        if (v > 0.0) { tFlood += dt; vFlood += v * dt; }
        v = swmm_getValueExpanded(swmm_NODE, swmm_NODE_INFLOW, o1, -1, -1);
        vOut += v * dt;
        if (!si && v >= 0.001) { sumOutQ += v; nFlow++; sumOutQT += v * dt; tFlow += dt; }
        if (!si)
        {
            c = swmm_getValueExpanded(swmm_NODE, swmm_NODE_POLLUTANT_CONCENTRATION, o1, -1, 0);
            load += v * c * dt * 28.317 / 453592.37;   /* cfs*mg/L*s -> lbs */
        }
        if (t <= 0.0) break;
    }
    swmm_end();   /* also writes the statistics report, as in a normal run */

    swmm_getNodeStats(j1, &ns);
    swmm_getLinkStats(c1, &ls);
    os.totalLoad = outLoad;
    swmm_getOutfallStats(o1, &os);
    swmm_getSubcatchStats(s1, &ss);
    swmm_getSystemRunoffTotals(&rt);
    swmm_getSystemRoutingTotals(&ft);
    for (k = 0; k < SWMM_MAX_FLOW_CLASSES; k++) sumClass += ls.timeInFlowClass[k];

    if (!si)
    {
        printf("%s (US units), %ld routing steps\n", inp, nSteps);
        printf("  %-34s %14s %14s\n", "statistic", "getter", "expected");
        checkMean("J1 avgDepth", ns.avgDepth, sumDepth / nSteps, sumDepthT / routed, "ft");
        check("J1 timeFlooded", ns.timeFlooded, tFlood / 3600.0, "hours");
        check("C1 sum of timeInFlowClass", sumClass, routed / 3600.0, "hours");
        checkMean("O1 avgFlow", os.avgFlow, nFlow ? sumOutQ / nFlow : 0.0,
                  tFlow > 0.0 ? sumOutQT / tFlow : 0.0, "cfs");
        /* SWMM converts mg to lb with 2.203e-6 (Ucf[MASS]), 0.07 % below the
           exact 1/453592.37 used here, so this row is allowed 1 % */
        checkTol("O1 totalLoad TSS", outLoad[0], load, "lbs", 1.0e-2);
        check("S1 precip", ss.precip, 0.375, "in");
        check("RunoffTotals rainfall", rt.rainfall, 0.375, "in");
    }
    else
    {
        printf("%s (SI units), %ld routing steps\n", inp, nSteps);
        printf("  %-34s %14s %14s\n", "statistic", "getter", "expected");
        check("J1 maxDepth", ns.maxDepth, maxDepth, "m");
        check("J1 volFlooded", ns.volFlooded, vFlood, "m3");
        check("S1 precip", ss.precip, 9.375, "mm");
        check("RoutingTotals outflow", ft.outflow, vOut, "m3");
    }
    swmm_close();
    return 0;
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    if (runDeck("API-04_us.inp", 0) || runDeck("API-04_si.inp", 1)) return 1;
    if (nbad)
    {
        printf("FAIL: %d of 11 statistics differ from their definition (sums for averages, "
               "seconds for hours, volumes for depths, ft/cfs/ft3 in an SI project)\n", nbad);
        return 1;
    }
    printf("PASS: every statistic checked matches its definition, in the project's units\n");
    return 0;
#else
    printf("PASS: not affected, 5.2.4 has no statistics getters\n");
    return 0;
#endif
}
