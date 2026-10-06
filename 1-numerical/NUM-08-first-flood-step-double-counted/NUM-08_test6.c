/*
 * NUM-08 for 6.0.0: the same check as NUM-08_test.c through the 6.0.0 C API.
 *
 * DWSolver::commitNodeDepthState() (DynamicWave.cpp) keeps the legacy rule
 * for a node that floods without ponding:
 *     nodes.overflow[ui] = dV / dt;
 *     nodes.volume[ui]   = t.rpt_full_volume;
 * so on the step the storage first floods, the volume that fills it is
 * booked as storage and again as overflow.
 *
 * Correct behaviour (continuity): over every step, the change in stored
 * volume plus the overflow volume equals the step's net inflow volume,
 * 0.5*(q_old + q_new)*dt; the routing continuity error is ~0.
 * Tolerances as in NUM-08_test.c: 1% of a step's 3,000 ft3 inflow, and a
 * 0.5% continuity error; the defect gives thousands of ft3 and several %.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

#define MAXSTEPS 200

int main(void)
{
    double elapsed = 0.0, contErr = 0.0;
    double t[MAXSTEPS], q[MAXSTEPS], v[MAXSTEPS], ovf[MAXSTEPS];
    int rc, n = 0, k, s1, onset = -1;
    double worst = 0.0, worstIn = 0.0;

    SWMM_Engine e = swmm_engine_create();
    rc = swmm_engine_open(e, "NUM-08_open-tank-floods.inp", "NUM-08_6.rpt",
                          "NUM-08_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    s1 = swmm_node_index(e, "S1");
    t[0] = 0.0; q[0] = 0.0; v[0] = 0.0; ovf[0] = 0.0;
    if (!rc) {
        swmm_node_get_inflow(e, s1, &q[0]);
        swmm_node_get_volume(e, s1, &v[0]);
        n = 1;
    }
    while (!rc && n < MAXSTEPS)
    {
        rc = swmm_engine_step(e, &elapsed);
        if (elapsed <= 0.0) break;
        t[n] = elapsed * 86400.0;
        swmm_node_get_inflow(e, s1, &q[n]);       /* cfs */
        swmm_node_get_volume(e, s1, &v[n]);       /* ft3 */
        swmm_node_get_overflow(e, s1, &ovf[n]);   /* cfs */
        n++;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_get_routing_continuity_error(e, &contErr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }
    contErr *= 100.0;   /* fraction -> % */

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
    printf("Routing continuity error: %.3f %%\n", contErr);

    if (fabs(worst) > 30.0 || fabs(contErr) > 0.5)
    {
        printf("FAIL: a step books %.0f ft3 more storage+overflow than its "
               "%.0f ft3 inflow; routing continuity error %.3f %%\n",
               worst, worstIn, contErr);
        return 1;
    }
    printf("PASS: every step's storage change plus overflow equals its inflow; "
           "continuity error %.3f %%\n", contErr);
    return 0;
}
