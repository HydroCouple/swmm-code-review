/*
 * CRASH-12: under STEADY routing, findSFLinkQual() indexes the Link array
 * with the index of the conduit's upstream NODE.
 *
 * 5.3.0 added an API mass-flux block to findSFLinkQual() that reads
 * Link[j].apiExtQualMassFlux[p] and updates Link[j].totalLoad[p], where
 * j = Link[i].node1 is a node index. It runs for every conduit at every
 * routing step whenever the model has a pollutant.
 *
 * Scenario B (5.3.0 only, needs the 5.3.0 link flux API): all indices are in
 *   range. A mass flux is set on conduit C1 only. Correct: C1 carries
 *   pollutant (its concentration is > 0). With the bug C1 reads the flux of
 *   link 0 (C2, which is 0) and C2 receives C1's flux, so C1 stays at 0.
 * Scenario A (5.2.4 and 5.3.0): the outfall is listed first, so the upstream
 *   node of C2 has index 2 and Link[2] is past the end of the 2-element array.
 *   Correct: the run finishes with a small quality continuity error (1 cfs at
 *   10 mg/L through two conduits). Steady-flow quality routing itself leaves
 *   1.7% on this deck in 5.2.4, which has no flux block, so the check allows
 *   5%. With the bug the sanitizer stops the run at the first routing step;
 *   without a sanitizer the read past the array returns garbage.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double elapsed = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, ok = 1;

    setvbuf(stdout, NULL, _IONBF, 0);   /* keep scenario B's lines if A aborts */
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    {
        int c1, c2, step = 0;
        double conc1 = 0.0, conc2 = 0.0;

        err = swmm_open("CRASH-12_two-conduits.inp", "CRASH-12_B.rpt", "CRASH-12_B.out");
        if (!err) err = swmm_start(1);
        c1 = swmm_getIndex(swmm_LINK, "C1");
        c2 = swmm_getIndex(swmm_LINK, "C2");
        while (!err)
        {
            err = swmm_step(&elapsed);
            if (elapsed <= 0.0) break;
            if (++step == 1)
                err = swmm_setValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_LATMASS_FLUX,
                                            c1, 0, 0, 10.0);
            conc1 = swmm_getValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_CONCENTRATION, c1, 0, 0);
            conc2 = swmm_getValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_CONCENTRATION, c2, 0, 0);
        }
        swmm_end();
        swmm_close();
        printf("Scenario B: flux set on C1 (link index %d), C2 is link index %d\n", c1, c2);
        printf("  final concentration  C1 = %.5f mg/L   C2 = %.5f mg/L   (error code %d)\n",
               conc1, conc2, err);
        if (err || conc1 <= 0.0)
        {
            printf("  -> the flux set on C1 did not reach C1\n");
            ok = 0;
        }
    }
#else
    printf("Scenario B: skipped (5.2.4 has no link pollutant flux API)\n");
#endif

    err = swmm_open("CRASH-12_outfall-first.inp", "CRASH-12_A.rpt", "CRASH-12_A.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    printf("Scenario A: run finished with error code %d, quality continuity error %.3f %%\n",
           err, qualErr);
    if (err || fabs(qualErr) > 5.0) ok = 0;

    if (!ok)
    {
        printf("FAIL: under STEADY routing the link quality step uses the wrong link\n");
        return 1;
    }
    printf("PASS: under STEADY routing each conduit's API flux is applied to that conduit, "
           "and a deck with node index >= number of links runs cleanly\n");
    return 0;
}
