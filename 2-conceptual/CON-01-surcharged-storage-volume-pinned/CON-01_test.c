/*
 * CON-01: a closed storage unit (surcharge depth > 0) keeps its volume at
 * full volume above full depth, but its depth moves through the surcharge
 * band as if the band held water.
 *
 * storage_getVolume() (node.c) returns fullVolume for any depth at or above
 * full depth: the unit has a lid. initNodeStates() (dynwave.c) still gives
 * the node its curve area above full depth, and setNodeDepth() integrates
 * the depth with that area whenever it uses the free-surface update there:
 * always under SURCHARGE_METHOD SLOT, and under EXTRAN when no link responds
 * to head (5.3.0's #149 change; 5.2.4 freezes the depth instead). Up to
 * A x SurDepth of water is then held in a band that the volume never books.
 *
 * Decks (all tanks: full depth 10 ft, surcharge depth 5 ft):
 *   CON-01_closed-tank-fill.inp   S1, 1000 ft2, no links, 10 cfs for 1 h
 *   CON-01_closed-tank-drain.inp  S1 starts at 15 ft (full), 1 cfs withdrawn
 *   CON-01_slot-tank.inp          S2, 2000 ft2, inflow and outlet pipes,
 *                                 SLOT, 20 cfs for 2 h
 *
 * Correct behaviour (continuity): the routing continuity error is ~0, and the
 * drained tank ends with its full volume minus the water withdrawn.
 * Tolerances: |continuity error| < 1%, final drained volume within 50 ft3.
 * The band holds 5,000 ft3 (fill: 14% of the inflow), 3,600 ft3 (drain) and
 * 10,000 ft3 (SLOT: 7% of the inflow); a lidded tank leaves at most
 * MIN_SURFAREA x 5 ft = 63 ft3 unbooked, as a junction does.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static int runDeck(const char* inp, const char* rpt, const char* out,
                   const char* tankName, double* vWithdrawn, double* vFinal)
{
    double elapsed = 0.0, tPrev = 0.0, dt, lat, ovf, tFlood = -1.0;
    double vFlood = 0.0, yEnd = 0.0, vEnd = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, s;

    *vWithdrawn = 0.0;
    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    s = swmm_getIndex(swmm_NODE, tankName);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        dt = (elapsed - tPrev) * 86400.0;
        tPrev = elapsed;
        lat = swmm_getValue(swmm_NODE_LATFLOW, s);           /* cfs */
        if (lat < 0.0) *vWithdrawn -= lat * dt;               /* ft3 */
        ovf = swmm_getValue(swmm_NODE_OVERFLOW, s);           /* cfs */
        vFlood += ovf * dt;
        if (ovf > 0.0 && tFlood < 0.0) tFlood = elapsed * 1440.0;
        yEnd = swmm_getValue(swmm_NODE_DEPTH, s);
        vEnd = swmm_getValue(swmm_NODE_VOLUME, s);
    }
    swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    if (err) { printf("%-30s run stopped with error %d\n", inp, err); return 0; }

    *vFinal = vEnd;
    if (tFlood >= 0.0)
        printf("%-30s %8.2f %10.0f %10.0f %10.1f %9.3f\n", inp, yEnd, vEnd,
               vFlood, tFlood, flowErr);
    else
        printf("%-30s %8.2f %10.0f %10.0f %10s %9.3f\n", inp, yEnd, vEnd,
               vFlood, "-", flowErr);
    return fabs(flowErr) < 1.0;
}

int main(void)
{
    double vw, vf, vw2, vf2;
    int ok1, ok2, ok3, ok2v;

    printf("%-30s %8s %10s %10s %10s %9s\n", "deck", "depth", "volume",
           "flooded", "floods at", "cont.err");
    printf("%-30s %8s %10s %10s %10s %9s\n", "", "(ft)", "(ft3)", "(ft3)",
           "(min)", "(%)");
    ok1 = runDeck("CON-01_closed-tank-fill.inp", "CON-01_fill.rpt",
                  "CON-01_fill.out", "S1", &vw, &vf);
    ok2 = runDeck("CON-01_closed-tank-drain.inp", "CON-01_drain.rpt",
                  "CON-01_drain.out", "S1", &vw2, &vf2);
    ok3 = runDeck("CON-01_slot-tank.inp", "CON-01_slot.rpt",
                  "CON-01_slot.out", "S2", &vw, &vf);

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
