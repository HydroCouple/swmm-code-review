/*
 * CON-18: with SURCHARGE_METHOD SLOT, a surcharged conduit's flow area is
 * A(y) = aFull + (y - yFull) * w(y), a rectangle with the slot width at the
 * current depth, instead of the area under the slot-width profile that node
 * continuity uses. With the Sjoberg width w = 0.5423 * wMax * exp(-(y/yFull)^2.4)
 * this area shrinks as the depth rises from 1.29 to 1.78 times the full depth,
 * and above that it holds far less water than the slot width says.
 *
 * Deck: a horizontal 3 ft pipe, 1000 ft long, starts exactly full between two
 * 20 ft2 storage units J1 and J2. A constant 0.035 cfs enters J1 for 6 hours.
 * The only outlet is a weir 15 ft above J2's invert, which the water never
 * reaches, so every cubic foot that enters is stored and the head rises the
 * whole time.
 *
 * Correct behaviour:
 *  1. The water stored in the conduit cannot fall while the head along it
 *     rises. The test reads the conduit volume at each 5-minute report period
 *     from the binary output file and requires that it never falls below an
 *     earlier value by more than 1 ft3 (single-precision output).
 *  2. Conservation: conduit volume + storage-unit volumes must equal the
 *     initial volume plus the inflow so far. The test checks this at each
 *     report period, within 2% of the 6-hour inflow of 756 ft3 (the bug loses
 *     about 60% of it by the end).
 * It also prints SWMM's own routing continuity error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

int main(void)
{
    const double D = 3.0, L = 1000.0, PI = 3.14159265358979;
    const double vFull = PI * D * D / 4.0 * L;   /* conduit volume when just full */
    const double aStor = 20.0, qIn = 0.035;      /* storage area (ft2), inflow (cfs) */
    const double v0 = vFull + 2.0 * aStor * D;   /* initial volume: full pipe, both units at 3 ft */
    double elapsed = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    float *vol = NULL, *h1 = NULL, *h2 = NULL;
    int err, nPer = 0, len, i, j1, j2, c1, iDrop = 0;
    const double inTotal = qIn * 6.0 * 3600.0;  /* 756 ft3 */
    double runMax = -1.0, drop = 0.0, eta, tSec, stored, inflow, miss, worstMiss = 0.0;
    SMO_Handle out = NULL;

    err = swmm_open("CON-18_closed-pipe.inp", "CON-18.rpt", "CON-18.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    j2 = swmm_getIndex(swmm_NODE, "J2");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    /* conduit volume and node heads at each report period */
    if (SMO_init(&out) || SMO_open(out, "CON-18.out") ||
        SMO_getTimes(out, SMO_numPeriods, &nPer) ||
        SMO_getLinkSeries(out, c1, SMO_flow_volume, 0, nPer, &vol, &len) ||
        SMO_getNodeSeries(out, j1, SMO_hydraulic_head, 0, nPer, &h1, &len) ||
        SMO_getNodeSeries(out, j2, SMO_hydraulic_head, 0, nPer, &h2, &len))
    {
        printf("FAIL: could not read CON-18.out\n");
        return 1;
    }

    printf("Time   head / D   conduit volume   inflow so far   stored - initial   missing\n");
    printf("                      (ft3)            (ft3)            (ft3)          (ft3)\n");
    /* the series calls return periods startPeriod .. endPeriod-1 (len values);
       period i is at (i + 1) report steps of 300 s */
    for (i = 0; i < len; i++)
    {
        tSec = 300.0 * (i + 1);
        eta = 0.5 * (h1[i] + h2[i]) / D;
        if (vol[i] > runMax) runMax = vol[i];
        if (runMax - vol[i] > drop) { drop = runMax - vol[i]; iDrop = i; }
        inflow = qIn * tSec;
        stored = vol[i] + aStor * (h1[i] + h2[i]) - v0;
        miss = inflow - stored;
        if (fabs(miss) > fabs(worstMiss)) worstMiss = miss;
        if ((i + 1) % 6 == 0 || i == len - 1)
            printf("%d:%02d    %5.3f      %8.1f        %8.1f          %8.1f       %7.1f\n",
                   (int)(tSec / 3600), (int)(tSec / 60) % 60, eta, vol[i], inflow, stored, miss);
    }
    printf("Largest fall of the conduit volume below an earlier value: %.1f ft3 "
           "(reached at %.3f D)\n", drop, 0.5 * (h1[iDrop] + h2[iDrop]) / D);
    printf("Largest volume missing from storage: %.1f ft3 (%.1f%% of the 6-hour inflow)\n",
           worstMiss, 100.0 * worstMiss / inTotal);
    printf("SWMM routing continuity error: %.3f%%\n", flowErr);
    SMO_free((void **)&vol); SMO_free((void **)&h1); SMO_free((void **)&h2);
    SMO_close(&out);

    /* 1 ft3: output noise; the bug loses ~150 ft3 between 1.29 and 1.78 D.
       2% of the inflow: the bug loses up to ~60% of it. */
    if (drop > 1.0 || fabs(worstMiss) > 0.02 * inTotal)
    {
        printf("FAIL: the conduit volume falls by %.1f ft3 while the head rises, and up to "
               "%.1f ft3 of the inflow is missing from storage (continuity error %.3f%%)\n",
               drop, worstMiss, flowErr);
        return 1;
    }
    printf("PASS: the conduit volume rises with the head and storage accounts for "
           "the inflow (continuity error %.3f%%)\n", flowErr);
    return 0;
}
