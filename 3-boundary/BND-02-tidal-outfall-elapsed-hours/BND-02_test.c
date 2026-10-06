/*
 * BND-02 (legacy toolkit API, 5.2.4 and 5.3.0): a TIDAL outfall looks up its
 * curve at the hours elapsed since the start of the run, not at the hour of
 * the day.
 *
 * The input reference defines a tidal curve as "tidal height (i.e., outfall
 * stage) versus hour of day" ([OUTFALLS] Tcurve, [CURVES] TIDAL). The deck
 * starts at 06:00 and runs for 24 h; its gated outfall O1 (invert 0) has the
 * curve 0 h: 0 ft, 6 h: 4 ft, 12 h: 0 ft, 18 h: 2 ft, 24 h: 0 ft, and nothing
 * flows, so the outfall stage must equal the curve at the clock hour.
 *
 * Correct behaviour: at every routing step the outfall head equals the linear
 * interpolation of the curve at the clock time of the end of the step (the
 * time the stage is computed for). Tolerance 0.01 ft; the bug is 6 h out of
 * phase and misses by up to 4 ft.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

static const double hr[5] = {0, 6, 12, 18, 24}, st[5] = {0, 4, 0, 2, 0};

static double tide(double h)            /* curve value at clock hour h */
{
    int i;
    for (i = 1; i < 5; i++)
        if (h <= hr[i]) return st[i-1] + (st[i] - st[i-1]) * (h - hr[i-1]) / (hr[i] - hr[i-1]);
    return st[4];
}

int main(void)
{
    double t = 0.0, hours, clock, head, expect, diff, maxDiff = 0.0, worstClock = 0.0;
    double worstHead = 0.0, worstExpect = 0.0;
    int err, of, n = 0, lastHour = -1;

    err = swmm_open("BND-02_tide-from-0600.inp", "BND-02.rpt", "BND-02.out");
    if (!err) err = swmm_start(1);
    of = swmm_getIndex(swmm_NODE, "O1");
    printf("Clock   Expected   O1 head\n        (ft)       (ft)\n");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        hours = t * 24.0;                       /* elapsed hours */
        clock = fmod(6.0 + hours, 24.0);        /* the run starts at 06:00 */
        head = swmm_getValue(swmm_NODE_HEAD, of);
        expect = tide(clock);
        diff = fabs(head - expect);
        n++;
        if ((int)hours != lastHour)             /* first step of each hour */
        {
            lastHour = (int)hours;
            if (lastHour % 3 == 0 || lastHour == 1)
                printf("%02d:%02d   %6.3f    %6.3f\n", (int)clock,
                       (int)(fmod(clock, 1.0) * 60.0), expect, head);
        }
        if (diff > maxDiff)
        {
            maxDiff = diff; worstClock = clock; worstHead = head; worstExpect = expect;
        }
    }
    swmm_end();
    swmm_close();

    if (err || n == 0)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    if (maxDiff > 0.01)
    {
        printf("FAIL: the tide is out of phase with the clock: at %02d:%02d the outfall stage is "
               "%.3f ft, the curve gives %.3f ft\n", (int)worstClock,
               (int)(fmod(worstClock, 1.0) * 60.0), worstHead, worstExpect);
        return 1;
    }
    printf("PASS: the outfall stage follows the tidal curve by hour of the day "
           "(largest difference %.4f ft)\n", maxDiff);
    return 0;
}
