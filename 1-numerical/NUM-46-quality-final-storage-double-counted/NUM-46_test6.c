/*
 * NUM-46 for 6.0.0: mass left in a storage unit (or conduit) when it dries
 * out is added to Final Stored Mass twice in 5.2.4 and 5.3.0.
 *
 * 6.0.0 books the dried-out residue once, directly into a run total
 * (qual_routing_final_dry), so this test is expected to pass unpatched.
 *
 * The 5.2.4/5.3.0 mechanism, for reference:
 *
 * When a node or link dries, qualrout.c books its remaining pollutant MASS
 * with massbal_addToFinalStorage(), which adds it to
 * StepQualTotals[p].finalStorage. massbal_updateRoutingTotals() folds that
 * into the run total with no time-step factor, and it is called twice with
 * the same step totals: at the end of step n and again at the start of
 * step n+1, before the step totals are cleared. So every dried-out residue
 * is counted twice.
 *
 * Deck: a closed pond holding 200 ft3 at 100 mg/L (1.248 lb) dries out by
 * evaporation after about 5.8 h; nothing enters or leaves it. Fixed 10 s
 * DYNWAVE step.
 *
 * Correct behaviour (conservation of mass): evaporation removes water, not
 * pollutant, so Final Stored Mass equals Initial Stored Mass and the quality
 * continuity error is about 0. The test requires Final/Initial within 2 %
 * and a reported error within 2 % (correct runs give about -0.2 %, 0.998;
 * the bug gives 2.0 and -99.6 %).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

/* First-pollutant value of a line of the Quality Routing Continuity table */
static double qual_line(const char *rpt, const char *label)
{
    FILE *f = fopen(rpt, "r");
    char line[512];
    int inTable = 0;
    double v = NAN;
    if (!f) return NAN;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Quality Routing Continuity")) inTable = 1;
        if (inTable && strstr(line, label))
        {
            char *p = strstr(line, "..");   /* skip the dot leader */
            if (p) { while (*p == '.') p++; sscanf(p, "%lf", &v); }
            break;
        }
    }
    fclose(f);
    return v;
}

int main(void)
{
    double t = 0.0, v0 = -1.0, vEnd = -1.0, init, fin, err, ratio;
    int rc, su1;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "NUM-46_pond-dries-by-evaporation.inp", "NUM-46_6.rpt", "NUM-46_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    su1 = swmm_node_index(e, "SU1");
    if (!rc) swmm_node_get_volume(e, su1, &v0);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (rc || t <= 0.0) break;
        swmm_node_get_volume(e, su1, &vEnd);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    init = qual_line("NUM-46_6.rpt", "Initial Stored Mass");
    fin  = qual_line("NUM-46_6.rpt", "Final Stored Mass");
    err  = qual_line("NUM-46_6.rpt", "Continuity Error");
    ratio = fin / init;
    printf("Pond volume, start / end (ft3) ..... %10.3f / %.3f\n", v0, vEnd);
    printf("Initial Stored Mass (lb) ........... %10.3f\n", init);
    printf("Final Stored Mass (lb) ............. %10.3f\n", fin);
    printf("Final / Initial .................... %10.3f\n", ratio);
    printf("Reported continuity error (%%) ...... %10.3f\n", err);

    if (!(fabs(ratio - 1.0) < 0.02) || !(fabs(err) < 2.0))
    {
        printf("FAIL: nothing left the pond, but Final Stored Mass is %.3f lb for %.3f lb "
               "initially (x%.3f) and the continuity error is %.3f %%\n", fin, init, ratio, err);
        return 1;
    }
    printf("PASS: the dried-out pollutant mass is booked once (Final Stored = Initial Stored)\n");
    return 0;
}
