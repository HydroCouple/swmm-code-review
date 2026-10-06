/*
 * NUM-54: with SURCHARGE_METHOD SLOT, 5.2.4's momentum equation divides the
 * flow by the slot-inflated area to get the velocity used in the friction
 * term, while the hydraulic radius in the same term excludes the slot.
 *
 * Deck: a horizontal 3 ft circular pipe, n = 0.015, 1000 ft long, runs full
 * from a very large storage unit held at 20 ft to an outfall with a FIXED
 * stage of 10 ft. The pipe's mid-length head is 15 ft, five pipe diameters
 * (eta = 5) above the invert, so the Preissmann slot is active along the
 * whole pipe. NUM-54_extran.inp is the same pipe with SURCHARGE_METHOD EXTRAN.
 *
 * Correct behaviour: at steady state the head loss is all friction, so the
 * flow is the Manning full-pipe flow for the friction slope (h1 - h2)/L:
 *     Q = 1.486/n * A * R^(2/3) * sqrt((h1 - h2)/L) = 57.8 cfs
 * whichever surcharge method is used; the slot only adds storage. The test
 * computes Q from the heads the engine itself holds at the end of the run
 * and requires the pipe flow to match it within 1%. The slot-area velocity
 * gives about 5% too much flow at eta = 5 (the slot adds 5.1% to the area).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static int runDeck(const char *inp, const char *rpt, const char *out,
                   double *q, double *h1, double *h2)
{
    double elapsed = 0.0;
    int err, t1, o1, c1;

    err = swmm_open((char *)inp, (char *)rpt, (char *)out);
    if (!err) err = swmm_start(1);
    if (err) { swmm_close(); return err; }
    t1 = swmm_getIndex(swmm_NODE, "T1");
    o1 = swmm_getIndex(swmm_NODE, "O1");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        *q  = swmm_getValue(swmm_LINK_FLOW, c1);
        *h1 = swmm_getValue(swmm_NODE_HEAD, t1);
        *h2 = swmm_getValue(swmm_NODE_HEAD, o1);
    }
    swmm_end();
    swmm_close();
    return err;
}

int main(void)
{
    const double D = 3.0, n = 0.015, L = 1000.0, PI = 3.14159265358979;
    const double A = PI * D * D / 4.0, R = D / 4.0;
    double qS = 0, h1S = 0, h2S = 0, qE = 0, h1E = 0, h2E = 0, qaS, qaE, eS, eE;
    int err = 0;

    err |= runDeck("NUM-54_slot.inp", "NUM-54_slot.rpt", "NUM-54_slot.out", &qS, &h1S, &h2S);
    err |= runDeck("NUM-54_extran.inp", "NUM-54_extran.rpt", "NUM-54_extran.out", &qE, &h1E, &h2E);
    if (err)
    {
        printf("FAIL: a run stopped with an error\n");
        return 1;
    }
    qaS = 1.486 / n * A * pow(R, 2.0 / 3.0) * sqrt((h1S - h2S) / L);
    qaE = 1.486 / n * A * pow(R, 2.0 / 3.0) * sqrt((h1E - h2E) / L);
    eS = qS / qaS - 1.0;
    eE = qE / qaE - 1.0;

    printf("Surcharge   head T1   head O1   Manning full-pipe Q   SWMM Q    error\n");
    printf("method        (ft)      (ft)         (cfs)            (cfs)\n");
    printf("SLOT        %6.3f    %6.3f      %9.3f          %7.3f   %+6.2f%%\n", h1S, h2S, qaS, qS, 100.0 * eS);
    printf("EXTRAN      %6.3f    %6.3f      %9.3f          %7.3f   %+6.2f%%\n", h1E, h2E, qaE, qE, 100.0 * eE);

    /* 1%: the slot-area velocity gives +5.1% here, the conveyance area ~0% */
    if (fabs(eS) > 0.01 || fabs(eE) > 0.01)
    {
        printf("FAIL: a full pipe under a fixed head difference carries %.3f cfs with "
               "SURCHARGE_METHOD SLOT, %+.2f%% off the Manning full-pipe flow %.3f cfs "
               "(EXTRAN %+.2f%%)\n", qS, 100.0 * eS, qaS, 100.0 * eE);
        return 1;
    }
    printf("PASS: the surcharged pipe carries the Manning full-pipe flow with both "
           "SLOT and EXTRAN\n");
    return 0;
}
