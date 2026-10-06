/*
 * CON-01 for 6.0.0: the same check as CON-01_test.c through the 6.0.0 C API.
 *
 * DWSolver::initNodeStates() (DynamicWave.cpp) gives a closed storage unit
 * its curve area above full depth while node::getVolume() caps the volume
 * at the full volume, so the free-surface depth update (SLOT; EXTRAN with
 * sumdqdh = 0) holds water in the surcharge band that is never booked.
 * 6.0.0 also books a tank that starts above full depth at the curve volume
 * of its initial depth (15,000 ft3 here), because SWMMEngine::initialize()
 * computes the initial volume before the full volume that caps it.
 *
 * Correct behaviour and tolerances as in CON-01_test.c: |continuity error|
 * < 1% in all three decks, and the drained tank ends at its full volume
 * (10,000 ft3) minus the water withdrawn, within 50 ft3.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

static int runDeck(const char* inp, const char* rpt, const char* out,
                   const char* tankName, double* vWithdrawn, double* vFinal)
{
    double elapsed = 0.0, tPrev = 0.0, dt, lat, ovf, tFlood = -1.0;
    double vFlood = 0.0, yEnd = 0.0, vEnd = 0.0, contErr = 0.0;
    int rc, s;

    *vWithdrawn = 0.0;
    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    s = swmm_node_index(e, tankName);
    while (!rc)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        dt = (elapsed - tPrev) * 86400.0;
        tPrev = elapsed;
        swmm_node_get_lateral_inflow(e, s, &lat);            /* cfs */
        if (lat < 0.0) *vWithdrawn -= lat * dt;               /* ft3 */
        swmm_node_get_overflow(e, s, &ovf);                   /* cfs */
        vFlood += ovf * dt;
        if (ovf > 0.0 && tFlood < 0.0) tFlood = elapsed * 1440.0;
        swmm_node_get_depth(e, s, &yEnd);
        swmm_node_get_volume(e, s, &vEnd);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_get_routing_continuity_error(e, &contErr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("%-30s run stopped with error %d\n", inp, rc); return 0; }
    contErr *= 100.0;   /* fraction -> % */

    *vFinal = vEnd;
    if (tFlood >= 0.0)
        printf("%-30s %8.2f %10.0f %10.0f %10.1f %9.3f\n", inp, yEnd, vEnd,
               vFlood, tFlood, contErr);
    else
        printf("%-30s %8.2f %10.0f %10.0f %10s %9.3f\n", inp, yEnd, vEnd,
               vFlood, "-", contErr);
    return fabs(contErr) < 1.0;
}

int main(void)
{
    double vw, vf, vw2, vf2;
    int ok1, ok2, ok3, ok2v;

    printf("%-30s %8s %10s %10s %10s %9s\n", "deck", "depth", "volume",
           "flooded", "floods at", "cont.err");
    printf("%-30s %8s %10s %10s %10s %9s\n", "", "(ft)", "(ft3)", "(ft3)",
           "(min)", "(%)");
    ok1 = runDeck("CON-01_closed-tank-fill.inp", "CON-01_fill6.rpt",
                  "CON-01_fill6.out", "S1", &vw, &vf);
    ok2 = runDeck("CON-01_closed-tank-drain.inp", "CON-01_drain6.rpt",
                  "CON-01_drain6.out", "S1", &vw2, &vf2);
    ok3 = runDeck("CON-01_slot-tank.inp", "CON-01_slot6.rpt",
                  "CON-01_slot6.out", "S2", &vw, &vf);

    /* the drained tank starts full (10,000 ft3) and loses what is withdrawn */
    printf("Drain: withdrawn %.0f ft3, expected final volume %.0f ft3, "
           "booked %.0f ft3\n", vw2, 10000.0 - vw2, vf2);
    ok2v = fabs(vf2 - (10000.0 - vw2)) <= 50.0;

    if (!(ok1 && ok2 && ok2v && ok3))
    {
        printf("FAIL: the surcharge band holds water the tank volume does not "
               "book, in the %s%s%sdeck(s) (see table)\n",
               ok1 ? "" : "fill ", (ok2 && ok2v) ? "" : "drain ",
               ok3 ? "" : "SLOT ");
        return 1;
    }
    printf("PASS: closed storage units conserve water through the surcharge "
           "band in all three decks\n");
    return 0;
}
