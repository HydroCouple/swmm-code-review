/*
 * NUM-08: on the step a non-ponding storage node first floods, the fill
 * volume is booked twice.
 *
 * getFloodedDepth() (dynwave.c), canPond == FALSE branch:
 *     Node[i].overflow  = dV / dt;           // whole net inflow of the step
 *     Node[i].newVolume = Node[i].fullVolume;
 * If the node started the step below full, the part of dV that filled it
 * (fullVolume - oldVolume) is counted as a storage gain and again as
 * overflow.
 *
 * Deck: open storage S1, 1000 ft2 x 10 ft (10,000 ft3), no links, 100 cfs
 * for 5 min then nothing, fixed 30 s step. The tank fills, floods, and stops
 * flooding before the run ends.
 *
 * Correct behaviour (continuity): over every step, the change in stored
 * volume plus the overflow volume equals the net inflow volume of the step,
 * 0.5*(q_old + q_new)*dt (the trapezoid dynwave.c uses for dV). Over the
 * run, the routing continuity error is ~0.
 *
 * Tolerances: the flooding-onset step books up to fullVolume (10,000 ft3)
 * twice, i.e. an imbalance of thousands of ft3 against 3,000 ft3 of inflow
 * per step; a correct step balances to rounding. The pass limits (1% of the
 * step inflow, and 0.5% routing continuity error) are far from both.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#define MAXSTEPS 200

int main(void)
{
    double elapsed = 0.0, tPrev = 0.0;
    double t[MAXSTEPS], q[MAXSTEPS], v[MAXSTEPS], ovf[MAXSTEPS];
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, n = 0, k, s1, onset = -1;
    double worst = 0.0, worstIn = 0.0;

    err = swmm_open("NUM-08_open-tank-floods.inp", "NUM-08.rpt", "NUM-08.out");
    if (!err) err = swmm_start(1);
    s1 = swmm_getIndex(swmm_NODE, "S1");
    t[0] = 0.0;
    q[0] = 0.0; v[0] = 0.0; ovf[0] = 0.0;
    if (!err) {
        q[0] = swmm_getValue(swmm_NODE_INFLOW, s1);
        v[0] = swmm_getValue(swmm_NODE_VOLUME, s1);
        n = 1;
    }
    while (!err && n < MAXSTEPS)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        t[n]   = elapsed * 86400.0;
        q[n]   = swmm_getValue(swmm_NODE_INFLOW, s1);     /* cfs */
        v[n]   = swmm_getValue(swmm_NODE_VOLUME, s1);     /* ft3 */
        ovf[n] = swmm_getValue(swmm_NODE_OVERFLOW, s1);   /* cfs */
        n++;
    }
    swmm_end();
    if (!err) swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    printf("  time   inflow    volume  overflow   dV+ovf*dt   inflow vol   imbalance\n");
    printf("   (s)    (cfs)     (ft3)     (cfs)       (ft3)        (ft3)       (ft3)\n");
    for (k = 1; k < n; k++)
    {
        double dt = t[k] - t[k-1];
        double in = 0.5 * (q[k-1] + q[k]) * dt;
        double out = (v[k] - v[k-1]) + ovf[k] * dt;
        double imb = out - in;
        if (onset < 0 && ovf[k] > 0.0) onset = k;
        if (fabs(imb) > fabs(worst)) { worst = imb; worstIn = in; }
        if (k <= 12)
            printf("%6.0f %8.2f %9.1f %9.2f %11.1f %12.1f %11.1f%s\n",
                   t[k], q[k], v[k], ovf[k], out, in, imb,
                   k == onset ? "   <-- first flooding step" : "");
    }
    printf("Routing continuity error: %.3f %%\n", flowErr);

    /* 1% of the 3,000 ft3 inflow of a full step; 0.5% continuity */
    if (fabs(worst) > 30.0 || fabs(flowErr) > 0.5)
    {
        printf("FAIL: a step books %.0f ft3 more storage+overflow than its "
               "%.0f ft3 inflow; routing continuity error %.3f %%\n",
               worst, worstIn, flowErr);
        return 1;
    }
    printf("PASS: every step's storage change plus overflow equals its inflow; "
           "continuity error %.3f %%\n", flowErr);
    return 0;
}
